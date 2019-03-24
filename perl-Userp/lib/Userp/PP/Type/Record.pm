package Userp::PP::Type::Record;
use Moo;
with 'Userp::PP::Type';

=head1 DESCRIPTION

A record is a sequence where the elements can each have their own delcared type, and each
element can be named with an identifier.  Records can be encoded as a fixed count of all its
elements, as a variable number of defined elements out of an available set, as a sequence of
ad-hoc key/value pairs, or as some mix of those.  In each case, the element names *must* be
unique (or undefined), and the application should not depend on the order of elements.

The Record type serves a dual role as a way to match encoding with low-level records like
C structs, or to store sparsely-encoded key/value pairs for larger records that are not
practical to encode or even declare in their entirety.   Records are not a generic key/value
dictionary though; identifiers must adhere to a restricted set of Unicode and cannot be generic
data.  (for that, just declare a Sequence of type (key,value), and handle the conversion to
a dictionary within the applicaion)

=head1 ATTRIBUTES

=head2 fields

An arrayref of C<< [Symbol, Type] >>.  Identifier may be C<undef> in which case the field
is understood to be "filler" or "reserved".  Values will be automatically encoded as zero, and
skipped over during decoding.

The order of this list matters for the encoding of the first N C<static_fields>.

=head2 static_fields

A count of how many elements of the C<fields> array will be written into every record.
Any field above this number will be available as a dynamic field.

=head2 other_fieldtype

The type to use for other ad-hoc fields.  Set to C<undef> to disallow ad-hoc fields.

=cut

has fields          => ( is => 'ro', required => 1 );
has static_fields   => ( is => 'ro' );
has other_fieldtype => ( is => 'ro' );

sub isa_rec { 1 }

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
