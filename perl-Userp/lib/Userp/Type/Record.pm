package Userp::Type::Record;
use Moo;
extends 'Userp::Type';
use Carp;

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

An array of field definitions, each of the form:

=over

=item name

A symbol specifying the field name.

=item type

The type of the field's value.  The type determines how many bytes the field occupies, and
whether it is dynamic-length.

=item placement

One of C<"Sequence">, C<"Optional">, or a bit offset from the start of the record.

Byte-offset fields come first (and may overlap) followed by sequential fields followed by
any dynamic or ad-hoc fields the user decides to encode.

=item idx

This is a calculated attribute of the field.  Fields will always be sorted as static, sequenced,
and then optional, but the order the fields are passed to the constructor will determine the
ordering within those groups.

=back

=head2 static_bitlen

The number of bits reserved for static fields.  This is only needed if you have a record with
extra padding (before the sequential or optional fields) in which no static fields are defined
yet.  If set, it is an error to define static fields beyond the end of the static area.

=head2 adhoc_field_type

The type to use for values of ad-hoc fields.  The default is the type C<Any>, which enables
ad-hoc fields of any type.  Set this to the empty Choice type to prevent ad-hoc fields.

=head2 align

See L<Userp::Type>

=head1 OTHER CONSTRUCTOR OPTIONS

The constructor takes attributes directly, but you can also use any of the following
field-defining shortcuts.  (and these shortcuts can be used to extend a previous record type)

=over

=item static_fields

An arrayref of C<< [ $name, $type, $bit_offset ] >>.  Adds or updates a field with static placement.

=item sequential_fields

An arrayref of C<< [ $name, $type ] >>. Adds or updates a field with C<Sequence> placement.

=item optional_fields

An arrayref of C<< [ $name, $type ] >>. Adds or updates a field with C<Optional> placement.

=back

=cut

has fields           => ( is => 'ro', init_arg => undef, default => sub { [] } );
has static_bitlen    => ( is => 'ro' );
has adhoc_field_type => ( is => 'ro' );

has _field_by_name   => ( is => 'rw' );
has _seq_field_idx   => ( is => 'rw' );
has _opt_field_idx   => ( is => 'rw' );

sub isa_Record { 1 }

sub BUILD {
	my ($self, $args)= @_;
	if ($args->{fields}) {
		$self->_add_field($_->{name}, $_->{type}, $_->{placement})
			for @{$args->{fields}};
	}
	if ($args->{static_fields}) {
		$self->_add_field(@$_)
			for @{$args->{static_fields}};
	}
	if ($args->{sequential_fields}) {
		$self->_add_field($_->[0], $_->[1], 'Sequence')
			for @{$args->{sequential_fields}};
	}
	if ($args->{optional_fields}) {
		$self->_add_field($_->[0], $_->[1], 'Optional')
			for @{$args->{optional_fields}};
	}
}

sub _add_field {
	my ($self, $name, $type, $placement)= @_;
	!ref($name) && Userp::Bits::is_valid_symbol($name)
		or croak "Invalid symbol value: '$name'";
	ref($type)->isa('Userp::Type')
		or croak "Type required for field '$name'";
	defined $placement
		or croak "Field '$name' lacks a 'placement' attribute";
	my $prev_idx= $self->_field_by_name->{$name};
	my $new_idx;
	my $field;
	...
}

1;
