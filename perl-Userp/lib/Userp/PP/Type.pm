package Userp::PP::Type;
use Moo::Role;

=head1 ATTRIBUTES

=head2 name

The name of this type.  Must be a valid identifier defined in the current Scope.

=head2 id

The numeric index of this type within the scope that defines it.

=head2 spec

The text specification for this type.

=head2 align

The alignment for values of this type.  Can be 'bit', or a power-of-2 which will adjust the
starting address with respect to the beginning of the data block.

Bit alignment only takes effect when the type does not begin with a variable-length scalar
component, and when the previous data value was also bit-aligned.  It causes the leftover
high-bits of the first bit-aligned value to be carried over as the low-bits of the following
bit-aligned value.

=head2 has_scalar_component

True of the encoding of the type begins with an unsigned integer value.

=head2 scalar_component_max

The maximum value of a type having a leading unsigned integer component, or undef if the
component has no upper bound.  (also undef if has_scalar_component is false)

=head2 sizeof

Number of octets used to encode this type, or C<undef> if the encoding has a variable length.

=head2 bitsizeof

A number of bits used to encode this type, or C<undef> if the encoding has a variable length.

=cut

has name       => ( is => 'ro', required => 1 );
has id         => ( is => 'ro', required => 1 );
has spec       => ( is => 'ro', required => 1 );

requires 'bitsizeof';
requires 'sizeof';
requires 'has_scalar_component';
requires 'scalar_component_max';
sub isa_int { 0 }
sub isa_ident { 0 }
sub isa_choice { 0 }
sub isa_seq { 0 }

sub _bitlen {
	my $x= shift;
	my $i= 0;
	while ($x > 0) {
		++$i;
		$x >>= 1;
	}
	return $i;
}

=head1 METHODS

=head2 new_from_spec

This parses the TypeSpec notation to construct anew type.

A quick review of TypeSpec:

  MyIntType   I(#0,#FFFFFFFF)
  MyEnumType  E(ValueType,EncodeLiteral,(Ident Val, Ident2 Val2, ...))
  MyUnionType U(Merge,(MemberType1,MemberType2,...))
  MySeqType   S(Len,NamedElems,BitPack,ElemSpec)
  MyAnyType   A

=cut

sub new_from_spec {
	
}

=head2 has_member_type

  $bool= $type->has_member_type($other_type);

This only returns true on Union types when the C<$other_type> is a member or sub-member.

=cut

sub has_member_type { 0 }

require Userp::PP::Type::Integer;
require Userp::PP::Type::Identifier;
require Userp::PP::Type::Choice;
require Userp::PP::Type::Sequence;
1;
