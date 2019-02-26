package Userp::PP::Type::Enum;
use Moo;
extends 'Userp::PP::Type';

=head1 ATTRIBUTES

=head2 members

An arrayref of [identifier,type,value]

=head2 encode_literal

If true, each value of the enum will be encoded literally rather than encoding an integer
selector.  This is only available if all the members are of the same type.  This allows you
to essentially have a type where I<some> of the possible values have identifiers, but the full
range of the type can still be encoded.  At decode time, the I<literal> encoded bytes of the
value will be used to look up the related identifier, and if there is no match the user will
see an identifier of NULL and need to decode the value as a normal value to make sense of it.

=head2 const_bitlen

Derived attribute - returns the number of bits needed to encode a value of this type.
This will either be the number of bits of the maximum member index, or if C<encode_literal> is
set it will be the C<const_bitlen> of the member type (which may be undef).

=cut

has members        => ( is => 'ro', required => 1 );
has encode_literal => ( is => 'ro' );

sub _build_const_bitlen {
	my $self= shift;
	return $self->encode_literal? $self->members->[0]{type}->const_bitlen
		: Userp::PP::Type::_bitlen($#{$self->members});
}

sub _build_discrete_val_count {
	my $self= shift;
	return $self->encode_literal? $self->members->[0]{type}->discrete_val_count
		: scalar @{$self->members};
}

1;
