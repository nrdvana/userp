package Userp::Type::Integer;
use Carp;
use Userp::Error;
use Userp::Bits;
use Moo;
extends 'Userp::Type';

=head1 DESCRIPTION

The Integer type can represent an infinite range of integers, a half-infinite range, or a
finite range.  As a special case, it can be encoded in 2's complement. (the normal encoding of
a double-infinite range uses a sign bit, and magnitude)

An Integer type can also have named values.  When decoded, the value will be both an integer
and a symbol.  This allows for the typical style of C enumeration, but to make a pure
enumeration of symbols (with no integer value), just add lots of Symbol values to a Choice
type.

=head1 SPECIFICATION

To specify an integer type as TypeSpec text, use either min/max notation:

  (min=#-8000 max=#7FFF)

or 2's complement bits notation:

  (bits=32)

To specify enumeration names for integer values, specify a list where the first element is the
starting numeric value, and then a list of identifiers which will receive increasing values.

  (bits=32 names=(Null Primary Secondary (Other -1)))

=head1 ATTRIBUTES

=head2 min

The lower end of the integer range, or undef to extend infinitely in the negative direction.

=head2 max

The upper end of the integer range, or undef to extend infinitely in the positive direction.

=head2 bits

If nonzero, this is the number of bits in which the integer will be encoded.  If C<min> is set
to a value less than zero the bits will be encoded as 2's complement, and sign-extended when
decoded.

When this attribute is set, the value is no longer considered to have an "initial scalar
component", and cannot be inlined into a Choice.

=head2 names

An arrayref of Symbol, or [Symbol, Value].  If value is omitted it is assumed to be the next
higher value after the previous, with the default starting from zero.  There may be more than
one name for a value.  This may be useful for encoding, but the decoder will only provide the
first matching name.

=cut

has min   => ( is => 'ro' );
has max   => ( is => 'ro' );
has bits  => ( is => 'ro' );
has names => ( is => 'ro' );

sub isa_Integer { 1 }

has effective_min   => ( is => 'rwp' );
has effective_max   => ( is => 'rwp' );
sub effective_bits { shift->bitlen }

has _name_by_val => ( is => 'lazy' );
has _val_by_name => ( is => 'lazy' );

sub _build__name_by_val {
	my $names= shift->names || [];
	my $v= -1;
	{ reverse map { ref $_ eq 'ARRAY'? ( $_->[0], ($v=$_->[1]) ) : ( $_ => ++$v ) } @$names }
}

sub _build__val_by_name {
	my $names= shift->names || [];
	my $v= -1;
	{ map { ref $_ eq 'ARRAY'? ( $_->[0], ($v=$_->[1]) ) : ( $_ => ++$v ) } @$names }
}

sub _check_min_max_bits {
	my ($min, $max, $bits)= @_;
	return 1 unless (defined $min? 1 : 0) + (defined $max? 1 : 0) + (defined $bits? 1 : 0) > 1;
	if (defined $bits) {
		if (($min||0) < 0 || ($max||0) < 0) {
			my ($min2s, $max2s)= Userp::Bits::twos_minmax($bits);
			return 0 if defined $min && ($min < $min2s || $min > $max2s);
			return 0 if defined $max && ($max < $min2s || $max > $max2s);
			return ($min||$max) <= ($max||$min);
		}
		else {
			my $unsigned_max= Userp::Bits::unsigned_max($bits);
			return 0 if defined $max && $max > $unsigned_max;
			return 0 if defined $min && $min > $unsigned_max;
			return ($min||$max||0) <= ($max||$min||0);
		}
	}
	else {
		return $min <= $max;
	}
}

sub _merge_self_into_attrs {
	my ($self, $attrs)= @_;
	$self->next::method($attrs);
	# If bits are given, it takes priority.  Min and max only preserved if they make sense.
	if ($attrs->{bits}) {
		$attrs->{min}= $self->min
			if !defined $attrs->{min} && defined $self->min
			&& _check_min_max_bits($self->min, undef, $attrs->{bits});
		$attrs->{max}= $self->max
			if !defined $attrs->{max} && defined $self->max
			&& _check_min_max_bits($attrs->{min}, $self->max, $attrs->{bits});
	}
	# Else use min and max, and only preserve bits if it still fits.
	else {
		$attrs->{min}= $self->min unless defined $attrs->{min};
		$attrs->{max}= $self->max unless defined $attrs->{max};
		$attrs->{bits}= $self->bits
			if !defined $attrs->{bits} && defined $self->bits
		   && _check_min_max_bits($attrs->{min}, $attrs->{max}, $self->bits);
	}
	if (defined $attrs->{names} && defined $self->names) {
		$attrs->{names}= [ @{$self->names}, @{$attrs->{names}} ];
	}
	else {
		$attrs->{names}= $self->names;
	}
}

sub BUILD {
	my $self= shift;
	my ($min, $max, $bits, $align)= ($self->min, $self->max, $self->bits, $self->align);
	if (defined $bits) {
		# If bits attribute is defined, value is encoded in a fixed number of bits.
		# Those bits are 2's complement if $min is negative or not set, unsigned otherwise.
		if (!defined $min || $min < 0) {
			my ($min2s, $max2s)= Userp::Bits::twos_minmax($bits);
			$min= $min2s unless defined $min;
			$max= $max2s unless defined $max;
			Userp::Error::Domain->assert_minmax($min, $min2s, $max, 'Integer min');
			Userp::Error::Domain->assert_minmax($max, $min, $max2s, 'Integer max');
		}
		else {
			my $max_unsigned= Userp::Bits::unsigned_max($bits);
			$min= 0 unless defined $min;
			$max= $max_unsigned unless defined $max;
			Userp::Error::Domain->assert_minmax($min, 0, $max, 'Integer min');
			Userp::Error::Domain->assert_minmax($max, $min, $max, 'Integer max');
		}
	}
	elsif (defined $min && defined $max) {
		# If min and max are both defined, bits is derived from the largest value that can be encoded
		Userp::Error::Domain->assert_minmax($max, $min, undef, 'Integer max');
		$bits= Userp::Bits::bitlen($max-$min);
		# If alignment is given and bits are not, round up to a multiple of the alignment
		if (defined $align && $align > -3) {
			$bits= Userp::Bits::roundup_bits_to_alignment($bits, $align);
		}
	}
	else {
		# variable-length integers are always byte-aligned or higher
		$align= 0 unless defined $align && $align > 0;
	}
	# If alignment not given, determine sensible default from bits
	if (!defined $align) {
		$align= -3;
		for (my $x= $bits; $x && !($x & 1); $x >>= 1, ++$align) {};
	}
	$self->_set_effective_min($min);
	$self->_set_effective_max($max);
	$self->_set_effective_align($align);
	$self->_set_bitlen($bits);
}

sub _register_symbols {
	my ($self, $scope)= @_;
	$scope->add_symbols(map { ref $_ eq 'ARRAY'? $_->[0] : $_ } @{$self->names})
		if $self->names;
	$self->next::method($scope);
}

1;
