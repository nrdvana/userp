package Userp::Encoder;
use Moo;
use Carp;
use Userp::Bits;

=head1 SYNOPSIS

  # Encode Integer types
  $enc->int(42);
  
  # Encode symbolic constants
  $enc->sym('True');
  
  # Select a specific sub-type of the current type
  $enc->sel($sub_type);
  
  # Encode Arrays
  $enc->begin_array(3)->int(1)->int(2)->int(3)->end_array;
  
  # Encode records
  $enc->begin_record->field('foo')->int(6)->field('bar')->int(2)->end_record;
  
  # Encode records with known order of fields
  $enc->sel($my_rec_type)->begin_record->int(6)->int(2)->end_record;
  
  # Encode any type with a known optimized conversion from float
  $enc->float(4.2);
  
  # Encode any type with a known optimized conversion from string
  $enc->str("Hello World");
  
=head1 DESCRIPTION

This class is the API for serializing data.  The behavior of the encoder is highly dependent
on the L</scope>, which defines the available defined types and symbol table.

The sequence of API calls depends on type of the value being encoded.  Here is a brief
overview:

=over

=item Integer

Userp Integer types can have a constrained range, and optional symbols enumerating some of the
values.  Calling C<int> will interpret the perl scalar as an integer, check whether it is in
the domain of this Integer type, then encode it.  Calling C<sym> will look for a named value
within this type and then encode the integer.  If the integer is out of bounds, or if no such
symbol exists, the operation dies with an exception, and the encoder state remains unchanged.

  $enc->int($int_value);
  $enc->sym($symbolic_name);   # for integer types with named values

=item Symbol

If the current type is Symbol, you may encode any symbolic value that exists in the L</scope>'s
symbol table.  If the C<scope> is not finalized, you may encode any string value and it will
be automatically added to the symbol table.

  $enc->sym($symbolic_name);

=item Choice (or Any)

Choice types offer a selection between several types or values.  Select a more specific type
with the C<sel> method.  Select a specific option of the Choice type by passing two arguments;
the choice type and the index of the option you want.

  $enc->sel($type)->...    # select a member type
  $enc->sel(undef, 3)->... # select option 3 of the choice
  $enc->sel($type, 7)->... # select a sub-type and its option both at once

The library may allow you to skip calling C<sel> if there is a member type that can encode the
next thing you ask to encode.  For instance, if you call L</int> when the current type is
C<Userp.Any>, it will automatically select the first Integer type of C<Userp.Int>.

=item Array (and String)

Declare the dimensions of an array having dynamic dimensions during C<begin_array>.
Then, write each element of the array, iterating right-most dimension first.
(you might call this row-major order, but Userp does not distinguish which array index is
the "row")  Then call C<end_array>.

  $enc->begin_array($dim_1, ...); # only dimensions not specified in the type
  for(...) {
    $enc->...(...); # call once for each element
  }
  $enc->end_array();

Between C<begin_array> and C<end_array>, the C<current_type> will be the declared type of the
arra's elements, switching to C<undef> after the final element has been encoded.  You may not
call C<end_array> until all elements ar ewritten.

Userp has special handling for some array types, like C<Userp.String> or C<Userp.ByteArray>:

  $enc->str($string);

This avoids the obviously unacceptable performance of writing each individual byte with a
method call.

=item Record

Optionally declare the fields you will write, then encode each field in that order.
If the fields are defined in the type and have a static order, you can avoid fragmented buffers
by specifying those fields in that same order.

  $enc->begin_record("foo","bar");
  $enc->int(3);
  $enc->int(4);
  $enc->end_record;

If you do not want to pre-declare your fields, you must specify which field you are encoding
before each element.  However, this can result in more fragmented buffers than pre-declaring
the fields.

  $enc->begin_record;
  $enc->field("foo")->int(3);
  $enc->field("bar")->int(4);
  $enc->end_record;

=item Type

In very few cases, you might want to encode a reference to a type.  (i.e. encoding a reference
to a type as a value)  This gets its own method:

  $enc->typeref($type);

=back

=head1 ATTRIBUTES

=head2 scope

The current L<Userp::Scope> which defines what types and symbols are available.

=head2 bigendian

If true, all fixed-length integer values will be written most significant byte first.
The default on this object is false, but encoders created by L<Userp::StreamWriter> will be set
to whatever the stream is using.

=head2 buffers

Arrayref of L<Userp::Buffer> objects into which the data is encoded.

=head2 parent_type

The type whose elements are currently being written.  When you call L</begin_array> the
C<current_type> becomes the C<parent_type>, and the array's element type becomes the
C<current_type>.  This does not change after calling C<sel>.

=head2 current_type

The type being written next.  This does change after calling C<sel>.

=cut

has scope        => ( is => 'ro', required => 1 );
has bigendian    => ( is => 'ro' );
has buffers      => ( is => 'rw', lazy => 1, default => sub { ["",3,0] } );
sub parent_type     { my $st= shift->_enc_stack->[-1]; $st? $st->{type} : undef }
has current_type => ( is => 'rwp', required => 1 );

has _type_vtable => ( is => 'rw', default => sub { {} } );
has _cur_align   => ( is => 'rw', default => 3 );
has _bitpos      => ( is => 'rw' );
has _enc_stack   => ( is => 'rw', default => sub { [] } );
has _sel_paths   => ( is => 'rw', default => sub { [] } );

sub _align {
	my ($self, $new_align)= @_;
	if ($new_align < 3) {
		# bit-packing.  If starting, _bitpos needs initialized.
		if ($self->_cur_align >= 3) {
			$self->{_bitpos}= 0;
		}
		# Else it is already initialized and needs aligned if > 0
		elsif ($new_align > 0) {
			my $mask= (1 << $new_align) - 1;
			$self->{_bitpos}= ($self->{_bitpos} + $mask) & ~$mask;
		}
	}
	elsif ($new_align > 3) {
		my $mul= 1 << ($new_align-3);
		my $remainder= length(${$self->buffer_ref}) & ($mul-1);
		${$self->buffer_ref} .= "\0" x ($mul-$remainder) if $remainder;
	}
	$self->_cur_align($new_align);
	$self;
}

sub _vtable_for {
	my ($self, $type)= @_;
	$self->_type_vtable->{$type->id} ||= (
		$type->isa_Integer? $self->_gen_Integer_encoder($type)
		: $type->isa_Symbol? $self->_gen_Symbol_encoder($type)
		: $type->isa_Choice? $self->_gen_Choice_encoder($type)
		: $type->isa_Array? $self->_gen_Array_encoder($type)
		: $type->isa_Record? $self->_gen_Record_encoder($type)
		: $self->_gen_Any_encoder($type) # $type->isa_Any
	);
}

=head1 METHODS

=head2 sel

  $enc->sel($type)->...
  $enc->sel(undef, $option)->...
  $enc->sel($type, $option)->...

Select a sub-type, option, or option-of-subtype of the current Choice.

Returns the encoder for convenient chaining.

=cut

sub sel {
	my ($self, $subtype, $option)= @_;
	my $type= $self->current_type;
	$type->isa_choice
		or croak "Cannot select sub-type of ".$type->name;
	# Later, can dig through sub-options for a match, but to start, only allow immediate child options.
	my $opt_node= $type->_option_tree->{$subtype->id}
		or croak "Type ".$subtype->name." is not a direct sub-option of ".$type->name;
	if ($opt_node->{values} || $opt_node->{ranges} || ($opt_node->{'.'} && defined $opt_node->{'.'}{mege_ofs})) {
		# Need to delay the encoding until the value is known
		push @{ $self->_sel_paths }, { type => $type, opt_node => $opt_node };
	}
	elsif ($opt_node->{'.'}) {
		# selection of whole type; can encode selector and forget past details
		$self->_encode_selector($opt_node->{'.'}{sel_ofs});
	}
	else { croak "BUG" }
	$self->_set_current_type($subtype);
	$self;
}

=head2 int

  $enc->int($int_value)->... # plain scalar or BigInt object

The C<$int_value> must be an integer within the domain of the L</current_type>.
Method dies if arguments are invalid or if a non-integer value is required at this point of the
stream.  Returns the encoder for convenient chaining.

=cut

sub int {
	my ($self, $val)= @_;
	my $type= $self->current_type;
	if (!$type) {
		croak "Expected end()" if @{ $self->_enc_stack || [] };
		croak "Can't encode after end of block";
	}
	my $enc_fn= $self->_vtable_for($type)->{int}
		or croak "Can't encode ".$type->name." using int() method";
	my $err= $enc_fn->($self, $val);
	$err->throw if defined $err;
	$self->_next;
}

=head2 str

  $enc->str($string)->...

Encode a string.  The type must be one of the known string types (i.e. Arrays of characters or
bytes), Symbol, an Integer with symbolic (enum) names, or a Choice including one of those.
If the type is 'Any', it will default to the first (lowest type id) string type in the current
Scope, else Symbol.

=cut

sub str {
	my ($self, $str)= @_;
	my $type= $self->current_type;
	if (!$type) {
		croak "Expected end()" if @{ $self->_enc_stack || [] };
		croak "Can't encode after end of block";
	}
	my $enc_fn= $self->_vtable_for($type)->{str}
		or croak "Can't encode ".$type->name." using str() method";
	my $err= $enc_fn->($self, $str);
	$err->throw if defined $err;
	$self->_next;
}

=head2 typeref

  $enc->typeref($type)->... # Type object

=cut

sub typeref {
	my ($self, $type)= @_;
	$self->_next;
}

=head2 begin

Begin encoding the elements of a sequence.

=head2 end

Finish encoding the elements of a sequence, returning to the parent type, if any.

=cut

sub begin {
	my $self= shift;
	my $type= $self->current_type;
	if (!$type) {
		croak "Expected end()" if @{ $self->_enc_stack || [] };
		croak "Can't encode after end of block";
	}
	my $begin_fn= $self->_vtable_for($type)->{begin}
		or croak "begin() is not relevant for type ".$type->name;
	my $err= $self->$begin_fn(@_);
	$err->throw if defined $err;
	return $self;
}

sub _next {
	my $self= shift;
	if ((my $context= $self->_enc_stack->[-1])) {
		my $err= $context->{next}->($self);
		$err->throw if defined $err;
	}
	else {
		$self->_set_current_type(undef);
	}
	return $self;
}

sub end {
	my $self= shift;
	my $context= $self->_enc_stack->[-1]
		or croak "Can't call end() without begin()";
	my $err= $context->{end}->($self);
	$err->throw if defined $err;
	$self->_next;
}

sub _encode_typeref {
	my ($self, $type_id)= @_;
	$type_id= $type_id->id if ref $type_id;
	$self->_encode_vqty($type_id);
}

sub _gen_Integer_encoder {
	my ($self, $type)= @_;
	my $min= $type->effective_min;
	my $max= $type->effective_max;
	my $align= $type->effective_align;
	my $bits= $type->effective_bits;
	my $bits_fn= Userp::Bits->can($self->bigendian? 'concat_bits_be' : 'concat_bits_le');
	my $vqty_fn= Userp::Bits->can($self->bigendian? 'concat_vqty_be' : 'concat_vqty_le');
	my %methods;
	# If a type has a declared number of bits, or if it has both a min and max value,
	# then it gets encoded as a fixed number of bits.  For the case where min and max
	# are defined and bits are not, the value is encoded as an offset from the minimum.
	if (defined $bits) {
		my $base= defined $type->bits? 0 : $min;
		$methods{int}= sub {
			return Userp::Error::Domain->new_minmax($_[1], $min, $max, 'Type "'.$type->name.'" value')
				if $_[1] < $min || $_[1] > $max;
			$_[0]->_align($align);
			$bits_fn->(${$_[0]->{buffer_ref}}, $_[0]->{_bitpos}, $bits, $_[1] - $base);
			return;
		};
	}
	# If the type is unlimited, but has a min or max, then encode as a variable quantity
	# offset from whichever limit was given.
	elsif (defined $min || defined $max) {
		$methods{int}= sub {
			return Userp::Error::Domain->new_minmax($_[1], $min, $max, 'Type "'.$type->name.'" value')
				if defined $min && $_[1] < $min or defined $max && $_[1] > $max;
			$_[0]->_align($align);
			$vqty_fn->(${$_[0]->{buffer_ref}}, $_[0]->{_bitpos}, defined $min? $_[1] - $min : $max - $_[1]);
			return;
		};
	}
	# If the type is fully unbounded, encode nonnegative integers as value * 2, and negative integers
	# as value * 2 + 1
	else {
		$methods{int}= sub {
			$_[0]->_align($align);
			$vqty_fn->(${$_[0]->{buffer_ref}}, $_[0]->{_bitpos}, $_[1] < 0? (-$_[1]<<1) | 1 : $_[1]<<1);
			return;
		};
	}
	# If a type has names, then calling $encoder->str($enum_name) needs to look up the value
	# for that name, and throw an error if it doesn't exist.
	if ($type->names && @{ $type->names }) {
		my $enc_int= $methods{int};
		$methods{str}= sub {
			my $val= $type->_val_by_name->{$_[1]};
			return Userp::Error::Domain->new(valtype => 'Type "'.$type->name.'" enum', value => $_[1])
				unless defined $val;
			$enc_int->($_[0], $val);
			return;
		};
	}
	return \%methods;
}

sub _gen_Choice_encoder {
	my ($self, $type)= @_;
	my $sel_fn= sub {
		
	};
	...
}

sub _gen_Array_encoder {
	my ($self, $array)= @_;
	my $vqty_fn= Userp::Bits->can($self->bigendian? 'concat_vqty_be' : 'concat_vqty_le');
	my $dim_type= $array->dim_type;
	my $dim_type_enc= $dim_type? $self->_vtable_for($dim_type)->{int}
		: sub { $vqty_fn->(${$_[0]{buffer_ref}}, $_[0]{_bitpos}, $_[1]) };
	my ($next_fn, $end_fn);
	my $begin_fn= sub {
		my $self= shift;
		my $elem_type= $array->elem_type;
		if (!$elem_type) {
			$elem_type= shift;
			if (ref($elem_type) && ref($elem_type)->isa('Userp::Type')) {
				$self->scope->type_by_id($elem_type->id) == $elem_type
					or croak "Type '".$elem_type->name."' is not in the current scope";
			}
			else {
				my $t= $self->scope->type_by_name($elem_type)
					or croak "No such type '$elem_type' (expected element type as first argument to begin()";
				$elem_type= $t;
			}
		}
		my @dim= @_;
		my @typedim= $array->dim? @{$array->dim} : ();
		# If the type specifies some dimensions, make sure they agree with supplied dimensions
		if (@typedim) {
			croak "Supplied ".@dim." dimensions for type ".$array->name." which defines ".@typedim." dimensions"
				unless @dim <= @typedim;
			# The dimensions given must match or fill-in spots in the type-declared dimensions
			for (my $i= 0; $i < @typedim; $i++) {
				$dim[$i]= $typedim[$i] unless defined $dim[$i];
				croak "Require dimension $i for type ".$array->name
					unless defined $dim[$i];
				croak "Dimension $i of array disagrees with type ".$array->name
					if defined $typedim[$i] && $dim[$i] != $typedim[$i];
			}
		}
		
		push @{ $self->_enc_stack }, {
			type => $array,
			dim => \@dim,
			elem_type => $elem_type,
			i => 0,
			n => scalar @dim,
			next => $next_fn,
			end => $end_fn,
		};
		
		$self->_align($array->align);
		# Encode the elem_type unless part of the array type
		$vqty_fn->(${$self->{buffer_ref}}, $self->{_bitpos}, $elem_type->id) unless $array->elem_type;
		# Encode the number of dimensions unless part of the array type
		$vqty_fn->(${$self->{buffer_ref}}, $self->{_bitpos}, scalar @dim) unless @typedim;
		# Encode the list of dimensions, for each that wasn't part of the array type
		my $n= 1;
		for (0..$#dim) {
			$n *= $dim[$_]; # TODO: check overflow
			next if $typedim[$_];
			$self->$dim_type_enc($dim[$_]);
		}
		
		$self->_enc_stack->[-1]{n}= $n; # now store actual number of array elements pending
		$self->_set_current_type($n? $elem_type : undef);
		return;
	};
	$next_fn= sub {
		my $self= shift;
		my $context= $self->_enc_stack->[-1];
		if (++$context->{i} < $context->{n}) {
			$self->_set_current_type($context->{elem_type});
		} else {
			$self->_set_current_type(undef);
		}
		return;
	};
	$end_fn= sub {
		my $self= shift;
		my $context= $self->_enc_stack->[-1];
		my $remain= $context->{n} - $context->{i};
		croak "Expected $remain more elements before end()" if $remain;
		pop @{ $self->_enc_stack };
		return;
	};
	return { begin => $begin_fn, end => $end_fn, next => $next_fn };
}

sub _gen_Record_encoder {
	my ($self, $record)= @_;
	my $vqty_fn= Userp::Bits->can($self->bigendian? 'concat_vqty_be' : 'concat_vqty_le');
	my $begin_fn= sub {
		my $self= shift;
		push @{ $self->_enc_stack }, { type => $record, pending => {} };
		return;
	};
	my $end_fn= sub {
		my $self= shift;
		...;
		return;
	};
	my $field_fn= sub {
		my ($self, $name)= @_;
		...;
		return;
	};
	my $next_fn= sub {
		...;
		return;
	};
	return { begin => $begin_fn, field => $field_fn, end => $end_fn, next => $next_fn };
}

sub _gen_Symbol_encoder {
	my ($self, $type)= @_;
	my $vqty_fn= Userp::Bits->can($self->bigendian? 'concat_vqty_be' : 'concat_vqty_le');
	my $str_fn= sub {
		my ($self, $val)= @_;
		if (my $sym_id= $self->scope->symbol_id($val)) {
			$vqty_fn->(${$_[0]{buffer_ref}}, $_[0]{_bitpos}, $sym_id << 1);
		}
		elsif (my ($prefix, $prefix_id)= $self->scope->find_symbol_prefix($val)) {
			$vqty_fn->(${$_[0]{buffer_ref}}, $_[0]{_bitpos}, ($prefix_id << 1) | 1);
			$val= substr($val, length $prefix);
			utf8::encode($val);
			$vqty_fn->(${$_[0]{buffer_ref}}, $_[0]{_bitpos}, length $val);
			${$self->buffer_ref} .= $val;
		}
		else {
			utf8::encode($val);
			$vqty_fn->(${$_[0]{buffer_ref}}, $_[0]{_bitpos}, 1);
			$vqty_fn->(${$_[0]{buffer_ref}}, $_[0]{_bitpos}, length $val);
			${$self->buffer_ref} .= $val;
		}
		return;
	};
	return { str => $str_fn };
}

sub _gen_Any_encoder {
	my ($self, $type)= @_;
	my $vqty_fn= Userp::Bits->can($self->bigendian? 'concat_vqty_be' : 'concat_vqty_le');
	my $align= $type->align;
	my $sel_fn= sub {
		# Verify this type is in scope
		$_[0]->scope->type_by_id($_[1]->id) == $_[1]
			or return Userp::Error::API->new(message => 'Type "'.$_[1]->name.'" is not in the current scope');
		# Encode the type selector
		$_[0]->_align($align);
		$vqty_fn->($_[0], $_[1]->id);
		# And now that type is the current type
		$_[0]->_set_current_type($_[1]);
		return;
	};
	my $int_fn= sub {
		my $int_t= $_[0]->scope->default_integer_type
			or return Userp::Error::API->new(message => 'No default Integer type specified in scope');
		$_[0]->_align($align);
		$vqty_fn->($_[0], $int_t->id);
		$_[0]->_vtable_for($int_t)->{int}->($_[0], $_[1]);
		return;
	};
	my $str_fn= sub {
		my $str_t= $_[0]->scope->default_string_type
			or return Userp::Error::API->new(message => 'No default String type specified in scope');
		$_[0]->_align($align);
		$vqty_fn->($_[0], $str_t->id);
		$_[0]->_vtable_for($str_t)->{str}->($_[0], $_[1]);
		return;
	};
	my $begin_fn= sub {
		my $seq_t= $_[0]->scope->default_sequence_type
			or return Userp::Error::API->new(message => 'No default sequence type (array/record) specified in scope');
		$_[0]->_align($align);
		$vqty_fn->($_[0], $seq_t->id);
		$_[0]->_vtable_for($seq_t)->{str}->($_[0], $_[1]);
		return;
	};
	return { sel => $sel_fn, int => $int_fn, str => $str_fn, begin => $begin_fn };
}

sub _encode_initial_scalar {
	my ($self, $scalar_value)= @_;
	my @to_encode;
	my $type= $self->current_type;
	my $val= $scalar_value;
	my $choice_path= $self->_choice_path;
	if (@$choice_path) {
		for (reverse @$choice_path) {
			defined (my $opt= $_->_option_for($type, $val))
				or croak "Options of Choice ".$_->name." do not include type ".$type->name." offset ".$val;
			$val= defined $opt->[1]? $val + $opt->[1]  # merged option, with adder
				: defined $opt->[2]? (($val << $opt->[2]) | $opt->[3])  # merged option, with shift/flags
				: do { # non-merged option, as single int.
					# Needs to be followed by encoding of $subtype, unless a single value was selected
					# from subtype.
					unshift @to_encode, [ $type, $val ] if $opt->[4] > 1;
					$opt->[0];
				};
			$type= $_;
		}
	}
	my $max= $type->scalar_component_max;
	$self->_encode_qty($val, defined $max? _bitlen($max) : undef);
	for (@to_encode) {
		$max= $_->[0]->scalar_component_max;
		$self->_encode_qty($_->[1], defined $max? _bitlen($max) : undef);
	}
	return $self;
}

sub _encode_block_tables {
	my $self= shift;
	my $buf= $self->buffers->[-1];
	croak "Type stack is not empty"
		if @{ $self->_enc_stack };
	
	# Need to encode bytes, not strings, so make a copy
	my @symbols= @{ $self->scope->_symbols };
	utf8::encode($_) for @symbols;
	# Number of entries in the symbol table
	$buf->encode_int(scalar @symbols);
	# length of each entry, not including NUL terminator
	$buf->encode_int(length $_) for @symbols;
	# Symbol data
	$buf->encode_bytes($_)->encode_bytes("\0") for @symbols;
	
	# Number of entries in the type table
	my @types= @{ $self->scope->_types };
	$buf->encode_int(scalar @types);
	my $typedef_type= $self->scope->type_Typedef;
	for (@types) {
		# Each type encodes itself as "Type Typedef"
		$self->_set_current_type($typdef_type);
		$_->_encode_definition($self);
		croak "Incomplete type encoding"
			if defined $self->current_type || @{ $self->_enc_stack };
	}
	for (@types) {
		# Type "Choice" can include literal values, which can't be processed
		# until all types have been loaded by the decoder.
		$_->_encode_constants($self);
	}
	# Tables are complete
	return $self;
}

1;
