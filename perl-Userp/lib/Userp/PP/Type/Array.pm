package Userp::PP::Type::Array;
use Moo;
with 'Userp::PP::Type';

=head1 DESCRIPTION

An array is composed of a list of elements identified only by their position in the list.
The elements of the list might be logically arranged as a single or multi-dimension array.
The elements of the list share a common type, though that type may be a Choice of other types.
The length of the sequence may be defined as part of the type, or left unspecified to be
decoded from the data of each instance.

=head1 ATTRIBUTES

=head2 elem_type

The type of all elements.  May be a Choice, including C<< scope->type_All >>.

=head2 dim_type

An Integer type to use when decoding dimensions which are written in-line with the data.
Normally, the dimensions of an array are written as variable-length integers.  By specifying
a fixed-width integer type this can make an array compatible with a C struct such as

  struct MyIntArray { uint32_t len; uint32_t buffer[] };

=head2 dim

An ArrayRef which defines the dimensions of the array.  If this attribute itself is undefined,
then the array has one dimension which will be encoded along with the data.  If the arrayref is
defined, each undefined element represents a dimension that will be encoded along with the data.

=cut

has elem_type => ( is => 'ro', required => 1 );
has dim_type  => ( is => 'ro', required => 1 );
has dim       => ( is => 'ro', required => 1 );

sub isa_seq { 1 }

1;
