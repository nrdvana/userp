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

=head2 len_const

Holds the constant value of C<len> if C<len> is a constant.

=head2 len_type

Holds the type reference for C<len> if C<len> is a type.

=head2 sparse

True if C<len> is set to C<'sparse'>.

=head2 nullterm

True if C<len> is set to C<'nullterm'>.

=head2 named_elems

Specifies whether or not the elements have unique identifiers.  If true, the sequence behaves
more like a record or map.  If false the sequence is more like an array.

=head2 elem_spec

Either a single Type for all elements, or an arrayref of Type or [Type,Ident] for each of the
first N elements.  If L</len> is not given or is longer than this list, additional elements
will have their identifier and type encoded in the data as needed.

=head2 bitpack

If true, then every time an element which can be encoded in fewer than its normal multiple-of-8
bits (but strictly less than 32 bits) is followed by another such element, the second element 
will have several of its low-end bits moved to the high-end of the byte before it.
(the encoder/decoder do not pretend to have any concept of "bit order" of the hardware, but
simply move least significant bits from the second element to the most significant bits of the
first element.)

=cut

has len         => ( is => 'ro', required => 1 );
has len_const   => ( is => 'lazy' );
has len_type    => ( is => 'lazy' );
has sparse      => ( is => 'lazy' );
has nullterm    => ( is => 'lazy' );
sub _build_len_type  { $_[0]->len && ref($_[0]->len) && ref($_[0]->len)->can('const_bitlen')? $_[0]->len : undef }
sub _build_len_const { defined $_[0]->len && !ref $_[0]->len && ($_[0]->len =~ /^[0-9]+$/)? $_[0]->len : undef }
sub _build_sparse    { $_[0]->len eq 'sparse' }
sub _build_nullterm  { $_[0]->len eq 'nullterm' }

has named_elems => ( is => 'ro', required => 1 );
has elem_spec   => ( is => 'ro', required => 1 );
has bitpack     => ( is => 'ro' );

sub _build_const_bitlen {
	my $self= shift;
	my $len= $self->const_len;
	return undef unless defined $len;
	return 0 unless $len;
	my $spec= $self->elem_spec;
	my $total= 0;
	if (ref $spec eq 'ARRAY') {
		return undef unless @$spec >= $len;
		for (0 .. $len-1) {
			my $type= $spec->[$_][0];
			defined (my $bits= $type->const_bitlen) or return undef;
			$bits= ($bits+7)&~7 unless $self->bitpack;
			$total += $bits;
		}
		return $total;
	}
	elsif (ref($spec)->can('const_bitlen')) {
		my $bits= $spec->const_bitlen;
		$bits= ($bits+7)&~7 unless $self->bitpack;
		return $bits * $len;
	}
	return undef;
}

1;
