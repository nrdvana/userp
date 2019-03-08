package Userp::PP::Type::Enum;
use Moo;
extends 'Userp::PP::Type';

=head1 ATTRIBUTES

=head2 value_type

The type of the values of the enum.  This should be the plain UInt type to get automatic
incrementing integers for each defined member.  You can also set it to any other type.

=head2 members

An arrayref of [identifier,subtype,value]

=head2 encode_literal

If true, each value of the enum will be encoded literally rather than encoding an integer
selector.  This is only available if all the members are Integers of the same type.
This allows you to essentially have a type where I<some> of the possible values have
identifiers, but the full range of the type can still be encoded. At decode time, the API user
will see that the current element is an Enum, but the identifier will be NULL and they will
have to decode it as an integer.

=head2 bitsizeof

Derived attribute - returns the number of bits needed to encode a value of this type.
This will either be the number of bits of the maximum member index, or if C<encode_literal> is
set it will be the C<bitsizeof> of the member type (which may be undef).

=cut

has value_type     => ( is => 'ro', required => 1 );
has members        => ( is => 'ro', required => 1 );
has encode_literal => ( is => 'ro' );

sub _build_bitsizeof {
	my $self= shift;
	return $self->encode_literal? $self->members->value_type->bitsizeof
		: Userp::PP::Type::_bitlen($#{$self->members});
}

sub _build_discrete_val_count {
	my $self= shift;
	return $self->encode_literal? $self->members->value_type->discrete_val_count
		: scalar @{$self->members};
}

1;
