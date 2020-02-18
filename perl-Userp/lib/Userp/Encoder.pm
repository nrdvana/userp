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

  $enc->int($int_value);
  $enc->sym($symbolic_name);   # for integer types with named values

=item Symbol

  $enc->str($symbolic_name);

=item Choice (or Any)

  $enc->sel($type)->...  # select a member (or sub-member) type

Depending on which types are available in the Choice, you might need to specify which sub-type
you are about to encode.  If the types are obvious, you can skip straight to the C<int> or
C<str> or C<type> or C<begin> method.

=item Array

  $enc->begin($length, ...); # length and further array dimensions may or may not be required
  for(...) {
    $enc->...(...); # call once for each element
  }
  $enc->end();

or, to encode anything that is conceptualy an array of integers, you can use the codepoints of
a string:

  $enc->str($string);

=item Record

  $enc->begin();
  for (my ($k, $v)= each %hash) {
    $enc->field($k)->...($v);
  }
  $enc->end();

=item Type

(i.e. encoding a reference to a type as a value)

  $enc->typeref($type);

=back

=head1 ATTRIBUTES

=head2 scope

The current L<Userp::Scope> which defines what types and identifiers are available.

=head2 bigendian

If true, all fixed-length integer values will be written most significant byte first.
The default on this object is false, but encoders created by L<Userp::Stream> might get a
platform-dependent default.

=head2 buffer_ref

Reference to the scalar that the userp Block data is written into.  It is a ref for efficiency
when operating on large blocks.

=head2 buffer

For convenience, this returns the value referenced by L</buffer_ref>.

=head2 current_type

The type about to be written next.  This should be initialized to the root type of the Block.
After that, it will track the state changes caused by L</sel>, L</begin> and L</end>.

=cut

has scope        => ( is => 'ro', required => 1 );
has bigendian    => ( is => 'ro' );
has buffer_ref   => ( is => 'rw', lazy => 1, default => sub { \(my $x="") } );
sub buffer          { ${ shift->buffer_ref } }
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

Select a sub-type of the current Choice.  This includes selecting any type at all when the
current type is Any.  This may be given multiple times to express specific choices within
choices, though doing so is not always required if the option path can be inferred.
Returns the encoder for convenient chaining.

=cut

sub sel {
	my ($self, $subtype)= @_;
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

1;
