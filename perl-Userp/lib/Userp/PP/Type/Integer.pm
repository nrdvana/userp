package Userp::PP::Type::Integer;
use Moo;
with 'Userp::PP::Type';

=head1 DESCRIPTION

The Integer type can represent an infinite range of integers, a half-infinite range, or a
finite range.  As a special case, it can be encoded in 2's complement.

An Integer type can also have named values.  When decoded, the value will be both an integer
and a symbol.  This allows for the typical style of C enumeration, but to make a pure
enumeration of symbols, just add lots of Identifier values to a Choice type.

=head1 ATTRIBUTES

=head2 min_val

The lower end of the integer range, or undef to extend infinitely in the negative direction.

=head2 max_val

The upper end of the integer range, or undef to extend infinitely in the positive direction.

=head2 twos_complement

If nonzero, this is the number of bits in which the integer will be encoded as 2's complement.

=head2 aliases

A hashref of C<< { int => Identifier } >>, optional.

=cut

has min_val         => ( is => 'ro' );
has max_val         => ( is => 'ro' );
has twos_complement => ( is => 'ro' );
has aliases         => ( is => 'ro' );

sub isa_int { 1 }

1;
