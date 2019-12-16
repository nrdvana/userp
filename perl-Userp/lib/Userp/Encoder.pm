package Userp::Encoder;
use Moo;
use Carp;
use Userp::Bits;

=head1 SYNOPSIS

  # Direct use of an Encoder object
  my $scope= Userp::Scope->new();
  my $enc= Userp::Encoder->new(scope => $scope, current_type => $scope->type_Any);
  $enc->sel($scope->type_Integer)->int(5);  # select Integer type, encode value '5'
  syswrite($fh, $enc->buffer);

=head1 DESCRIPTION

This class is the API for serializing data.  The behavior of the encoder is highly dependent
on the L</scope>, which defines the available defined types and symbol table.

The sequence of API calls depends on type of the value being encoded.  Here is a brief
overview:

=over

=item Integer

  $enc->int($int_value);
  $enc->str($symbolic_name);   # for integer types with named values

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
	if (!$type->isa_int) {
		croak "Can't encode ".$type->name." using int() method";
#	# If $type is a choice, search for an option compatible with this integer value
#	if ($type->isa_choice) {
#		my $opt_node= $type->_option_tree->{int}
#			or croak "Must select sub-type of ".$type->name." before encoding an integer value";
#		if ($opt_node->{values} && defined $opt_node->{values}{$val}) {
#			$self->_encode_selector($opt_node->{values}{$val});
#		}
#		elsif ($opt_node->
#
#|| $opt_node->{ranges} || ($opt_node->{'.'} && defined $opt_node->{'.'}{merge_ofs})) {
#			push @{ $self->_sel_paths }, { type => $type, opt_node => $opt_node };
#		}
#		elsif ($opt_node->{'.'}) {
#			$self->_encode_selector($opt_node->{'.'}{sel_ofs});
#		}
#		else { croak "BUG" }
#	}
#	$type->isa_int
#		or croak "Type ".$type->name." cannot encode integers";
	}
	# Type should be integer, here.

	$self->_encode_int($type, $val);
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
	my ($choicetype, $choiceiter);
	if ($type->isa_Choice) {
		$choicetype= $type;
		my @stack;
		$choiceiter= sub {
			...
		};
		$choiceiter->();
	}
	my $err;
	{
		if ($type->isa_Symbol) {
			# TODO: verify that the value matches the rules for a Symbol
			$self->_encode_symbol($str);
		}
		elsif ($type->isa_Integer) {
			if (defined (my $ival= $type->_val_by_name->{$str})) {
				$self->_encode_int($type, $ival);
			} else {
				$err= "Integer type '".$type->name."' does not have enum for '".$str."'";
			}
		}
		elsif ($type->isa_Array) {
			# either array of bytes, or array of codepoints
			# If bytes, need to encode string first
			...
		}
		elsif ($type->isa_Any) {
			$type= $self->scope->default_string_type;
			redo;
		}
		if (defined $err) {
			croak $err unless defined $choiceiter;
			$err= undef;
			redo if $choiceiter->();
			croak "No options of choice-type '".$choicetype."' can encode the value '$str'";
		}
	}
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
	if ($self->current_type->isa_ary) {
		my $array= $self->current_type;
		my $elem_type= $array->elem_type;
		if (!$elem_type) {
			$elem_type= shift;
			unless (ref($elem_type) && ref($elem_type)->isa('Userp::Type')) {
				my $t= $self->scope->type_by_name($elem_type)
					or croak "No such type '$elem_type' (expected element type as first argument to begin()";
				$elem_type= $t;
			}
		}
		my $dim_type= $array->dim_type || $self->scope->type_Integer;
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
		
		push @{ $self->_enc_stack }, { type => $array, dim => \@dim, elem_type => $elem_type, i => 0, n => scalar @dim };
		
		# Encode the elem_type unless part of the array type
		$self->_encode_typeref($elem_type) unless $array->elem_type;
		# Encode the number of dimensions unless part of the array type
		$self->_encode_vqty(scalar @dim) unless @typedim;
		# Encode the list of dimensions, for each that wasn't part of the array type
		my $n= 1;
		for (0..$#dim) {
			$n *= $dim[$_]; # TODO: check overflow
			next if $typedim[$_];
			if ($dim_type == $self->scope->type_Integer) {
				$self->_encode_vqty($dim[$_]);
			}
			else {
				local $self->{current_type}= $dim_type;
				$self->int($dim[$_]);
			}
		}
		
		$self->_enc_stack->[-1]{n}= $n; # now store actual number of array elements pending
		
		$self->_set_current_type($n? $elem_type : undef);
	}
	elsif ($self->current_type->isa_rec) {
		
	}
	else {
		croak "begin() is not relevant for type ".$self->current_type->name;
	}
}

sub _next {
	my $self= shift;
	my $context= $self->_enc_stack->[-1];
	if (!$context) {
		$self->_set_current_type(undef);
	}
	elsif (defined $context->{n}) {
		if (++$context->{i} < $context->{n}) {
			$self->_set_current_type($context->{elem_type});
		} else {
			$self->_set_current_type(undef);
		}
	}
	return $self;
}

sub end {
	my $self= shift;
	my $context= $self->_enc_stack->[-1];
	if (!$context) {
		croak "Can't call end() without an array or record context";
	}
	elsif (defined $context->{n}) {
		my $remain= $context->{n} - $context->{i};
		croak "Expected $remain more elements before end()" if $remain;
		pop @{ $self->_enc_stack };
	}
	$self->_next;
}

sub _encode_qty {
	my ($self, $bits, $value)= @_;
	$bits > 0 or croak "BUG: bits must be positive in encode_qty($bits, $value)";
	$value >= 0 or croak "BUG: value must be positive in encode_qty($bits, $value)";
	$self->bigendian
		? Userp::Bits::concat_bits_be(${$self->{buffer_ref}}, $self->{_bitpos}, $bits, $value)
		: Userp::Bits::concat_bits_le(${$self->{buffer_ref}}, $self->{_bitpos}, $bits, $value)
}

sub _encode_vqty {
	my ($self, $value)= @_;
	$value >= 0 or croak "BUG: value must be positive in encode_vqty($value)";
	$self->bigendian
		? Userp::Bits::concat_vqty_be(${$self->{buffer_ref}}, $self->{_bitpos}, $value)
		: Userp::Bits::concat_vqty_le(${$self->{buffer_ref}}, $self->{_bitpos}, $value)
}

sub _encode_typeref {
	my ($self, $type_id)= @_;
	$type_id= $type_id->id if ref $type_id;
	$self->_encode_vqty($type_id);
}

sub _encode_symbol {
	my ($self, $val)= @_;
	if (my $sym_id= $self->scope->symbol_id($val)) {
		$self->_encode_vqty($sym_id << 1);
	}
	elsif (my ($prefix, $prefix_id)= $self->scope->find_symbol_prefix($val)) {
		$self->_encode_vqty(($prefix_id << 1) | 1);
		$val= substr($val, length $prefix);
		utf8::encode($val);
		$self->_encode_vqty(length $val);
		${$self->buffer_ref} .= $val;
	}
	else {
		utf8::encode($val);
		$self->_encode_vqty(1);
		$self->_encode_vqty(length $val);
		${$self->buffer_ref} .= $val;
	}
}

sub _encode_int {
	my ($self, $type, $val)= @_;
	# validate min/max
	my $min= $type->min;
	my $max= $type->max;
	Userp::Error::Domain->assert_minmax($val, $min, $max, 'Type "'.$type->name.'" value');

	# Pad to correct alignment
	$self->_align($type->align);
	# If defined as 2's complement then encode as a fixed number of bits, signed if min is negative.
	if ($type->bits) {
		$self->_encode_qty($type->bits, $val & ((1 << $type->bits)-1));
	}
	# If min and max are both defined, encode as a quantity counting from $min
	elsif (defined $min && defined $max) {
		$self->_encode_qty($type->bitlen, $val - $min);
	}
	# Else encode as a variable unsigned displacement from either $min or $max, or if neither of those
	# are given, encode as the unsigned quantity shifted left to use bit 0 as sign bit.
	else {
		$self->_encode_vqty(
			defined $min? $val - $min
			: defined $max? $max - $val
			: $val < 0? (-$val<<1) | 1
			: $val<<1
		);
	}
}

#sub _encode_option_for_type {
#	my ($self, $subtype)= @_;
#	if 
#	my $type= $self->current_type;
#	my @options= @{$self->_options }
#	# If current type is "Any", just write the ID of the type
#	if ($type == $self->scope->type_Any) {
#		$
#	my $opts= $self->_choice_options;
#	(@$opts= grep { $_->{type} == $subtype or $_->{type}->isa_choice && $_->{type}->contains_type($subtype) } @$opts)
#		or croak "Type ".$self->_choice_type->name." does not contain sub-type ".$subtype->name;
#}
#sub _encode_option_for_int {
#}
#sub _encode_option_for_sym {
#}
#sub _encode_option_for_typeref {
#}
#
#
#		or croak "Type ".$type->name." is not a valid option for ".$self->current_type->name;
#	push @{ $self->_choice_path }, $self->current_type;
#	$self->_set_current_type($type);
#

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
