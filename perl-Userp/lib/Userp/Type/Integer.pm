package Userp::Type::Integer;
use Carp;
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

  (twosc=32)

To specify enumeration names for integer values, specify a list where the first element is the
starting numeric value, and then a list of identifiers which will receive increasing values.

  (twosc=32 names=(Null Primary Secondary (Other -1)))

=head1 ATTRIBUTES

=head2 min

The lower end of the integer range, or undef to extend infinitely in the negative direction.

=head2 max

The upper end of the integer range, or undef to extend infinitely in the positive direction.

=head2 twosc

If nonzero, this is the number of bits in which the integer will be encoded as 2's complement.
When this is true, the value is no longer considered to have an "initial scalar component", and
cannot be inlined into a Choice.

=head2 names

An arrayref of Symbol, or [Symbol, Value].  If value is omitted it is assumed to be the next
higher value after the previous, with the default starting from zero.  There may be more than
one name for a value.  This may be useful for encoding, but the decoder will only provide the
first matching name.

=cut

has min   => ( is => 'ro' );
has max   => ( is => 'ro' );
has twosc => ( is => 'ro' );
has names => ( is => 'ro' );

sub isa_int { 1 }

sub _merge_self_into_attrs {
	my ($self, $attrs)= @_;
	$self->next::method($attrs);
	if (defined $attrs->{twosc}) {
		# preserve min/max if they are within the range of the two's complement encoding
		# else they will get set to the defaults in the constructor.
		my $max= (1 << ($attrs->{twosc} - 1)) - 1;
		my $min= -$max - 1;
		$attrs->{max}= $self->max if !defined $attrs->{max} && defined $self->max && $self->max <= $max && $self->max >= $min;
		$attrs->{min}= $self->min if !defined $attrs->{min} && defined $self->min && $self->min >= $min && $self->min <= $max;
	}
	else {
		$attrs->{max}= $self->max unless defined $attrs->{max};
		$attrs->{min}= $self->min unless defined $attrs->{min};
		if ($self->twosc) {
			# If new min/max do not fit within two's complement, un-set the two's complement
			my $max= (1 << ($self->twosc - 1)) - 1;
			my $min= -$max - 1;
			$attrs->{twosc}= $self->twosc unless $attrs->{max} > $max || $attrs->{min} < $min;
		}
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
	croak "min > max (".$self->min." > ".$self->max.")"
		if defined $self->min && defined $self->max && $self->min > $self->max;
	if ($self->twosc) {
		my $max= (1 << ($self->twosc - 1)) - 1;
		my $min= -$max - 1;
		if (defined $self->max) {
			Userp::Error::Domain->assert_minmax($self->max, $min, $max, 'Integer max')
		} else {
			$self->{max}= $max;
		}
		if (defined $self->min) {
			Userp::Error::Domain->assert_minmax($self->min, $min, $max, 'Integer min');
		} else {
			$self->{min}= $min;
		}
	}
}

1;
