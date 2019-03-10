package Userp::PP::Type;
use Moo;

=head1 ATTRIBUTES

=head2 name

The name of this type.  Must be a valid identifier defined in the current Scope.

=head2 id

The numeric index of this type within the scope that defines it.

=head2 spec

The text specification for this type.

=head2 align

A number which the address of the encoded value (within the block) must be aligned to.

=head2 tail_align

A number which the end-address of the encoded value must be aligned to, by adding padding bytes.

=head2 sizeof

Number of octets used to encode this type, or C<undef> if the encoding has a variable length.

=head2 bitsizeof

A number of bits used to encode this type, or C<undef> if the encoding has a variable length.

=head2 discrete_val_count

An integer of the number of possible values that this type can represent, or C<undef> if there
is not a finite number.

=cut

has name       => ( is => 'ro', required => 1 );
has id         => ( is => 'ro', required => 1 );
has spec       => ( is => 'ro', required => 1 );
has align      => ( is => 'rwp' );
has tail_align => ( is => 'rwp' );

has bitsizeof          => ( is => 'lazy' );
sub sizeof                { (shift->bitsizeof+7) >> 3 }
has discrete_val_count => ( is => 'lazy' );

sub _build_bitsizeof {
	my $n= shift->discrete_val_count;
	return defined $n? _bitlen($n-1) : undef;
}
sub _build_discrete_val_count {
	undef;
}

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

=head2 new

Standard Moo constructor.

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
require Userp::PP::Type::Enum;
require Userp::PP::Type::Union;
require Userp::PP::Type::Sequence;
1;
