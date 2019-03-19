package Userp::PP::Type::Sequence;
use Moo;
with 'Userp::PP::Type';

=head1 DESCRIPTION

A sequence is composed of a list of elements identified only by their position in the list.
The elements of the list might be logically arranged as a single or multi-dimension array.
The elements of the list share a common type, though that type may be a Choice of other types.
The length of the sequence may be defined as part of the type, or left unspecified to be
decoded from the data of each instance.

=head1 ATTRIBUTES

=head2 elem_type

The type of all elements.  May be a Choice, including C<< scope->type_All >>.

=head2 dimension_type

Must be an integer type.  This will be used to encode the values of any dimension which was not
specified as a constant.

=head2 dimensions

An ArrayRef which defines the dimensions of the array.  If this attribute itself is undefined,
then the array dimensions will be encoded along with the data.  If the arrayref is defined,
each undefined element represents a dimension that will be encoded along with the data.

=cut

has elem_type       => ( is => 'ro', required => 1 );
has dimension_count => ( is => 'ro', required => 1 );
has dimension_type  => ( is => 'ro', required => 1 );
has dimensions      => ( is => 'ro', required => 1 );

sub isa_seq { 1 }

1;
