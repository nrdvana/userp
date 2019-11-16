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

If nonzero, this is the number of bits in which the integer will be encoded as 2's complement.
When this is true, the value is no longer considered to have an "initial scalar component", and
cannot be inlined into a Choice.

=head2 names

An arrayref of Symbol, or [Symbol, Value].  If value is omitted it is assumed to be the next
higher value after the previous, with the default starting from zero.  There may be more than
name for a value.  This may be useful for encoding, but the decoder will only provide the first
matching name.

=cut

has min   => ( is => 'ro' );
has max   => ( is => 'ro' );
has bits  => ( is => 'ro' );
has names => ( is => 'ro' );

sub isa_int { 1 }

1;
