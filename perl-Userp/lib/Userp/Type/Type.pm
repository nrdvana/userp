package Userp::Type::Type;
use Moo;
extends 'Userp::Type';

=head1 DESCRIPTION

The Type Type allows you to encode a reference to a type into the data.

=cut

sub isa_Type { 1 }
sub base_type { 'Type' }
sub effective_align { 0 }

1;
