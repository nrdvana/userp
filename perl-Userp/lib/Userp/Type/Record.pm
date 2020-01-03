package Userp::Type::Record;
use Moo;
extends 'Userp::Type';

=head1 DESCRIPTION

A record is a sequence where the elements can each have their own delcared type, and each
element is named with a unique identifier.  Record encoding handles four key types of field:
static-placement fields that occur at a specific byte offset, sequential fields that are always
present and packed end-to-end, optional fields that may or may not be present, and ad-hoc
fields that are not pre-declared and are encoded as key/value pairs.

The order of sequential and optional fields is part of the type definition, but the Userp
library allows out-of-order access.

The Record type serves a dual role as a way to match low-level encodings like C-language structs
but also handle looser key/value pairs like JavaScript objects.
Records are not a generic key/value dictionary though; field names must adhere to a restricted
set of Unicode and cannot be generic data.

=head1 ATTRIBUTES

=head2 fields

A set of field definitions, each of the form:

=over

=item name

A symbol specifying the field name.

=item type

The type of the field's value.  The type determines how many bytes the field occupies, and
whether it is dynamic-length.

=item placement

One of C<"Sequence">, C<"Optional">, or a byte offset from the start of the record.

Byte-offset fields come first (and may overlap) followed by sequential fields followed by
any dynamic or ad-hoc fields the user decides to encode.

=back

=head2 adhoc_field_type

The type to use for values of ad-hoc fields.  The default is the type C<Any>, which enables
ad-hoc fields of any type.  Set this to the empty Choice type to prevent ad-hoc fields.

=head2 align

See L<Userp::Type>

=cut

has fields           => ( is => 'ro' );
has adhoc_field_type => ( is => 'ro' );

sub isa_Record { 1 }

1;
