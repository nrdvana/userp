package Userp::PP::Type::Integer;
use Moo;
with 'Userp::PP::Type';

=head1 DESCRIPTION

The Integer type can represent an infinite range of integers, a half-infinite range, or a
finite range.  As a special case, it can be encoded in 2's complement. (the normal encoding of
a double-infinite range uses a sign bit, and magnitude)

An Integer type can also have named values.  When decoded, the value will be both an integer
and a symbol.  This allows for the typical style of C enumeration, but to make a pure
enumeration of symbols (with no integer value), just add lots of Identifier values to a Choice
type.

=head1 SPECIFICATION

To specify an integer type as TypeSpec text, use either min/max notation:

  (I min -8000 max #7FFF)

or 2's complement bits notation:

  (I bits 32)

To specify enumeration names for integer values, specify a list where the first element is the
starting numeric value, and then a list of identifiers which will receive increasing values.

  (I bits 32 names ((0 Null Primary Secondary)(-1 Other)))

=head1 ATTRIBUTES

=head2 min

The lower end of the integer range, or undef to extend infinitely in the negative direction.

=head2 max

The upper end of the integer range, or undef to extend infinitely in the positive direction.

=head2 bits

If nonzero, this is the number of bits in which the integer will be encoded as 2's complement.
When this is true, the value is no longer considered to have an "initial scalar component", and
cannot be inlined into a Choice.

=head2 names

A hashref of C<< { int => Identifier } >>, optional.

=cut

has min   => ( is => 'ro' );
has max   => ( is => 'ro' );
has bits  => ( is => 'ro' );
has names => ( is => 'ro' );

sub isa_int { 1 }

1;
