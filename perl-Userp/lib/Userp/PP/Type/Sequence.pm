package Userp::PP::Type::Sequence;
use Moo;
extends 'Userp::PP::Type';

=head1 ATTRIBUTES

=head2 len

If set to a number, it specifies the number of elements in the sequence (saving the need to
encode that per instance).  If set to a Type (of Integer) that integer type will be used to
encode/decode the number of elements.  If set to the special value 'sparse', the elements of
the sequence will each be prefixed with the index or identifier where they belong in the result.
If set to the special value 'nullterm', the array will be terminated by a type of zero or
identifier of zero or value of zero, depending on other settings.

=head2 named_elems

Specifies whether or not the elements have unique identifiers.  If true, the sequence behaves
more like a record or map.  If false the sequence is more like an array.

=head2 elem_spec

Either a single Type for all elements, or an arrayref of Type or [Ident,Type] for each of the
first N elements.  If L</len> is not given or is longer than this list, additional elements
will have their identifier and type encoded in the data as needed.

=cut

has len         => ( is => 'ro', required => 1 );
has named_elems => ( is => 'ro', required => 1 );
has elem_spec   => ( is => 'ro', required => 1 );

1;
