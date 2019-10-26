package Userp::PP::Encoder;
use Moo;
use Carp;
use Math::BigInt;
use Userp::Bits;

=head1 DESCRIPTION

This class is the API for serializing data into the types defined in the current scope.

The sequence of API calls depends on type of the value being encoded.  Here is a brief
overview:

=over

=item Integer

  $enc->int($int_value);
  $enc->sym($symbol);   # for integer types with named values

=item Symbol

  $enc->sym($symbol);   # symbol name or symbol object

=item Choice

  $enc->sel($some_type)->...  # select a member (or sub-member) type

Depending on which types are available in the Choice, you might need to specify which sub-type
you are about to encode.  If the types are obvious, you can skip straight to the 'int' or 'sym'
or 'type' or 'begin' method.

=item Array

  $enc->begin($length); # length may or may not be required
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

The current L<Userp::PP::Scope> which defines what types and identifiers are available.

=head2 bigendian

True for ecoding with most significant byte first, false for encoding with least significant
byte first.

=head2 current_type

The type expected for the current node.  If this is a Choice (including type Any) you can select
sub-types with method L</type>.

=cut

has scope        => ( is => 'ro', required => 1 );
has bigendian    => ( is => 'ro', required => 1 );

has _cur_align   => ( is => 'rw', default => 3 );
has _bitpos      => ( is => 'rw' );
has buffer       => ( is => 'rw' );

has current_type => ( is => 'rwp' );
has _sel_paths   => ( is => 'rw', default => sub { [] } );

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
Method dies if arguments are invalid or if a different type is required at this point of the
stream.  Returns the encoder for convenient chaining.

=cut

#sub int {
#	my ($self, $val)= @_;
#	my $type= $self->current_type;
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
#	# Type should be integer, here.
#	$type->isa_int
#		or croak "Type ".$type->name." cannot encode integers";
#	# validate min/max
#	my $min= $int_type->min;
#	my $max= $int_type->max;
#	defined $min and $val < $min and croak "Integer out of bounds for type ".$type->name.": $val < $min";
#	defined $max and $val > $max and croak "Integer out of bounds for type ".$type->name.": $val > $max";
#	if (
#	# If defined as "bits" (2's complement) then encode as a fixed number of bits, signed if min is negative.
#	if ($int_type->bits) {
#		@{ $self->_sel_paaths }
#		
#		$self->_encode_qty($val, $int_type->bits, $min < 0);
#	}
#	# Else encode as a variable unsigned displacement from either $min or $max, or if neither of those
#	# are given, encode as the unsigned quantity shifted left to use bit 0 as sign bit.
#	else {
#		$self->_encode_qty(
#			defined $min? $val - $min
#			: defined $max? $max - $val
#			: ($val < 0)? -($val<<1)-1 : ($val<<1)
#		);
#	}
#	$self;
#}

=head2 sym

  $enc->sym($symbol)->... # Symbol object or plain string

Encode a symbol.  The current type must be Any, a Symbol type, an Integer type with named
values, or a Choice that includes one or more of those.

=cut

sub sym {
	my ($self, $val)= @_;
	my $type= $self->current_type;
	# If $type is a choice, search for an option containing this symbol
	
	my $ident= $self->scope->find_ident($val);
	if ($type == $self->scope->type_Ident) {
		if (@{ $self->_choice_path }) {
			# Choice path can't include a dynamic identifier unless the identifier is encoded
			# separately.  So, if $ient->id is defined, then this encoding will have completely
			# encoded the value.  If it is not defined, then we need to "encode an identifier"
			# following the selector.
			$self->_encode_initial_scalar($ident? $ident->id : undef);
			if (!$ident) {
				my $base= $self->scope->find_longest_ident_prefix($val);
				$self->_encode_qty(($base->id << 1) + 1);
				$self->_append_bytes(substr($val, length $base->name)."\0");
			}
		}
	}
	elsif ($type->can('_option_for')) {
		# If type is Union, push subtype if Ident and try again.
		# TODO: try to DWIM and select any relevant Enum type before falling back to type=Ident.
		$self->type($self->scope->type_Ident)->encode_ident($val);
	}
	elsif ($type->can('value_by_name')) {
		defined (my $ival= $type->value_by_name($val))
			or croak "Type ".$type->name." does not enumerate a value for identifier ".$val;
		...
	}
	else {
		croak "Can't encode identifier as type ".$type->name;
	}
	$self;
}

=head2 typeref

  $enc->typeref($type)->... # Type object

=cut

sub typeref {
	my ($self, $type)= @_;
	$self;
}

=head2 begin

Begin encoding the elements of a sequence.

=head2 end

Finish encoding the elements of a sequence, returning to the parent type, if any.

=cut

sub begin {
	my ($self, $len)= @_;
	$self;
}

sub end {
	my $self= shift;
	$self;
}

=head2 str

=cut

sub str {
	my ($self, $str)= @_;
	$self;
}

sub _encode_qty {
	my ($self, $bits, $value)= @_;
	$bits > 0 or croak "BUG: bits must be positive in encode_qty($bits, $value)";
	$value >= 0 or croak "BUG: value must be positive in encode_qty($bits, $value)";
	Userp::Bits::pad_buffer_to_alignment(${$self->{buffer}}, $self->{_bitpos}, $self->{_cur_align});
	$self->bigendian
		? Userp::Bits::concat_bits_be(${$self->{buffer}}, $self->{_bitpos}, $bits, $value)
		: Userp::Bits::concat_bits_le(${$self->{buffer}}, $self->{_bitpos}, $bits, $value);
}

sub _encode_vqty {
	my ($self, $value)= @_;
	$value >= 0 or croak "BUG: value must be positive in encode_vqty($value)";
	Userp::Bits::pad_buffer_to_alignment(${$self->{buffer}}, $self->{_bitpos}, $self->{_cur_align} < 3? 3 : $self->{_cur_align});
	$self->bigendian
		? Userp::Bits::concat_vqty_be(${$self->{buffer}}, $self->{_bitpos}, $value)
		: Userp::Bits::concat_vqty_le(${$self->{buffer}}, $self->{_bitpos}, $value);
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
