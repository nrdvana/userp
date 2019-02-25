package Userp::PP::Type::Union;
use Moo;
extends 'Userp::PP::Type';

=head1 ATTRIBUTES

=head2 members

An array of types.

=head2 merge

Normally a union is encoded as a selector between its members, and then the value is encoded
according to its specific type.  If this setting is true, any member types which
are fixed-range integer types (and at most one which is an un-bounded integer type) will be
concatenated so that values can be encoded as a single integer of the combined range.
This can save a little space at the cost of encoding/decoding speed.

=cut

has members => ( is => 'ro', required => 1 );
has merge   => ( is => 'ro' );

1;
