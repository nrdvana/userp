package Userp::PP::Type::Enum;
use Moo;
extends 'Userp::PP::Type';

=head1 DESCRIPTION

An Enum type is a collection of possible values.  However, its purpose in the protocol is less
about type-theory and more about practical utility than it's name might suggest.

The first main use case for an enum is the obvious one: to enumerate a list of possible
meanings and encode each of them as a small number within the protocol.  In this case, the
values are "Ident" and the integer is hidden from the user.

The second use case for enums is as a sort of data compression.  The values enumerated might
be large strings or data structures, and by including them once within the type definition
they can then be repeated throughout the data in very few bytes.  In this case, a value doesn't
need to have any symbolic meaning; it can be decoded as a normal value.

The third use case for enums is to be compatible with the behavior used in many low-level
data structures, where some bit-width of integer is encoded and some of the values have special
meaning, though nothing enforces that only the special values are used.  In this case the enum
is encoded exactly as the underlying integer type, but a reader can inspect whether that value
is one of the special known values.

As a result, when encoding a union, you can specify a member index, or sometimes encode a whole
value of the C<value_type>, or sometimes specify a member by its symbolic identifier.
When decoding a union, the value can sometimes be read as an Ident but others times only a
value (and sometimes both).  Higher-level Userp implementations might also allow you to go
through the motions of encoding complex values and then automatically convert to the member
index with a matching value.

=head1 ATTRIBUTES

=head2 value_type

The type of the values of the enum.  This must be an integer type to use C<encode_literal>.
If this is type C<Any>, each member will need a type declared for it.  If it is type C<Ident>,
no type or value are needed per-member.

=head2 members

An arrayref of C<< [identifier,subtype,value] >>.  Every member must define Identifer or Value,
or both.  Identifier is required if C<value_type> is C<Ident>. Subtype is required if
C<value_type> is not an integer type.  Value is required if C<value_type> is anything other
than C<Ident>.

=head2 encode_literal

If true, each value of the enum will be encoded literally rather than encoding an integer
selector.  This is only available if C<value_type> is an integer type (incuding any of the 2's
complement integer types which are technically Unions).

=head2 bitsizeof

Derived attribute - returns the number of bits needed to encode a value of this type.
This will either be the number of bits of the maximum member index, or if C<encode_literal> is
set it will be the C<bitsizeof> of the member type (which may be undef).

=cut

has value_type     => ( is => 'ro', required => 1 );
has members        => ( is => 'ro', required => 1 );
has encode_literal => ( is => 'ro' );

has member_by_value => ( is => 'lazy' );
sub _build_member_by_value {
	my $self= shift;
	my $members= $self->members;
	return {
		map +($members->[$_][2] => $_),
			grep Userp::PP::_is_int($members->[$_][2]), reverse 0..$#$members
	};
}

has member_by_ident => ( is => 'lazy' );
sub _build_member_by_ident {
	my $members= $self->members;
	return {
		map +($members->[$_][0] => $_),
			grep defined $members->[$_][0], reverse 0..$#$members
	};
}

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
