package Userp::PP::Type::Sequence;
use Moo;
with 'Userp::PP::Type';

=head1 ATTRIBUTES

=head2 len

Sequence lengths are very flexible.  You can define the length as a constant, or define it as
a Type (which will always be read from within the data), as the special values "sparse" or
"nullterm", or leave it unspecified.
If unspecified, any time the type is referenced in a type definition or data it will be
followed by another opportunity to give this same length specifier.

=over

=item len=CONSTANT

If set to a constant, the array has a fixed length and no further options are available.

=item len=Type

If set to a Type, a value of that type will be read from the data block on each occurrence
of the array and used as the length,

=item len="sparse"

If set to the special value 'sparse', the elements of the sequence will each be prefixed with
the index or identifier where they belong in the result.

=item len="nullterm"

If set to the special value 'nullterm', the array will be terminated by a type of zero or
identifier of zero or value of zero, depending on other settings.

=item len="any"

In any reference within a meta block, you may specify the length as "any", and defer the length
specification until later.  In the data block, this value may only be given for values of type
"Type".

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

Either a single C<< { type => $type, align => $alignment, ident => $identifier } >> for all
elements, or an arrayref of them for each of the first N elements.  If L</len> is not given or
is longer than this list, additional elements will have their type and/or identifer encoded in
the data as needed.

=head3 Alignment

The fields of a sequence can be aligned to "bit" or power-of-two byte boundary.  If "bit"
alignent is selected, every time an element which can be encoded in fewer than its normal
multiple-of-8 bits is followed by another such element, the second element will have several of
its low-end bits moved to the high-end of the byte before it. (the encoder/decoder do not
pretend to have any concept of "bit order" of the hardware, but simply move least significant
bits from the second element to the most significant bits of the first element.)

If the alignment is a power of two, then the offset from the start of the block will be rounded
up to an even multiple of that number before encoding the element.

If you need to add alignment after the final element (say, to allow for SSE reads on the buffer
or something) just add one more element of type "reserved" and length 0, and give it the desired
starting alignment.

=cut

has len         => ( is => 'ro', required => 1 );
has len_const   => ( is => 'lazy' );
has len_type    => ( is => 'lazy' );
has sparse      => ( is => 'lazy' );
has nullterm    => ( is => 'lazy' );
sub _build_len_type  { $_[0]->len && ref($_[0]->len) && ref($_[0]->len)->can('bitsizeof')? $_[0]->len : undef }
sub _build_len_const { defined $_[0]->len && !ref $_[0]->len && ($_[0]->len =~ /^[0-9]+$/)? $_[0]->len : undef }
sub _build_sparse    { $_[0]->len eq 'sparse' }
sub _build_nullterm  { $_[0]->len eq 'nullterm' }

has named_elems => ( is => 'ro', required => 1 );
has elem_spec   => ( is => 'ro', required => 1 );

sub isa_seq { 1 }

sub has_scalar_component { !defined shift->len_const }

has sizeof    => ( is => 'lazy', builder => sub { shift->_calc_size } );
has bitsizeof => ( is => 'lazy', builder => sub { shift->sizeof << 3 } );

sub _build_sizeof {
	my ($self)= @_;
	# Need constant length
	my $len= $self->const_len;
	return undef unless defined $len;
	return 0 if !$len;
	my $spec= $self->elem_spec;
	if (ref $spec eq 'ARRAY') {
		return undef unless @$spec >= $len;
		my $bitsize= 0;
		for (0 .. $len-1) {
			my $type= $spec->[$_]{type};
			# each element must also be constant-width
			my $type_bits= $type->bitsizeof;
			defined $type_bits or return undef;
			# bit and byte alignment can be predicted; multibyte alignment can't
			my $align= $spec->[$_]{align} || 0;
			if ($align > 0) {
				return undef; # can't predict length
			} elsif ($align < 0) {
				# This field is bit-aligned.
				$bitsize += $type->bitsizeof;
			}
			else {
				# Byte aligned.  If prev was bit-aligned, round up
				$bitsize= ($bitsize + 7) & ~7 if $bitsize & 7;
				$bitsize += $type->sizeof << 3;
			}
		}
		return $bitsize;
	}
	else {
		my $type= $spec->{type};
		my $type_bits= $type->bitsizeof;
		defined $type_bits or return undef;
		my $align= $spec->{align} || 0;
		if ($align > 0) {
			return undef;
		}
		elsif ($align < 0) {
			return (($type_bits * $len) + 7) & ~7;
		}
		else {
			return (($type_bits + 7) & ~7) * $len;
		}
	}
}

1;
