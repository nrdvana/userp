package Userp::Type::Any;
use Moo;
extends 'Userp::Type';

=head1 DESCRIPTION

Type Any is a Choice of any defined type.  However, it needs some special handling because
it automatically grows as new types are defined, as opposed to a Choice type where the size
of the set (and selector within the protocol) is a constant.  Also, values of Any are always
encoded as a variable quantity with some special cases.

Type Any can be subclassed, but the only attributes to override are the L</align>ment and
L</metadata>.

=head1 ATTRIBUTES

See attributes of L<Userp::Type>.

=cut

sub isa_Any { 1 }

1;
