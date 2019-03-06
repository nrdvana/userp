package Userp::PP::Type;
use Moo;

=head1 ATTRIBUTES

=head2 id

The numeric index of this type within the scope that defines it.

=head2 ident_id

The IdentID of the name of this type within the scope that defines this type.

=head2 ident

The name of this type.

=head2 spec

The text specification for this type.

=head2 align

A number which the address of the encoded value (within the block) must be aligned to.

=head2 tail_align

A number which the end-address of the encoded value must be aligned to, by adding padding bytes.

=head2 const_bitlen

A number of bits used to encode this type, or C<undef> if the encoding has a variable length.

=head2 discrete_val_count

An integer of the number of possible values that this type can represent, or C<undef> if there
is not a finite number.

=cut

has ident      => ( is => 'ro', required => 1 );
has ident_id   => ( is => 'ro', required => 1 );
has spec       => ( is => 'ro', required => 1 );
has align      => ( is => 'rwp' );
has tail_align => ( is => 'rwp' );

has const_bitlen       => ( is => 'lazy' );
has discrete_val_count => ( is => 'lazy' );

sub _build_const_bitlen {
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

require Userp::PP::Type::Integer;
require Userp::PP::Type::Enum;
require Userp::PP::Type::Union;
require Userp::PP::Type::Sequence;
1;
