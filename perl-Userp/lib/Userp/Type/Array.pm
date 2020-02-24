package Userp::Type::Array;
use Moo;
extends 'Userp::Type';

=head1 DESCRIPTION

An array is composed of a list of elements identified only by their position in the list.
The elements of the list might be logically arranged as a single or multi-dimension array.
The elements of the list share a common type, though that type may be a Choice of other types.
The length of the sequence may be defined as part of the type, or left unspecified to be
decoded from the data of each instance.

=head1 ATTRIBUTES

=head2 elem_type

The type of all elements.  May be a Choice, including C<< $scope->type_Any >>.  If it is
C<undef>, the C<elem_type> will be encoded at the start of each array of this type.

=head2 dim_type

An Integer type to use when decoding dimensions which are written in-line with the data.
Normally, the dimensions of an array are written as variable-length integers.  By specifying
a fixed-width integer type this can make an array compatible with a C struct such as

  struct MyIntArray { uint32_t len; uint32_t buffer[]; };

  # The following sets up a compatible type:
  $uint32_t= Integer->new(bits => 32, min => 0);
  $MyIntArray= Array->new(elem_type => $uint32_t, dim_type => $uint32_t, dim => [undef]);

=head2 dim

An ArrayRef which defines the dimensions of the array.  If this attribute itself is undefined,
then the number of dimensions and dimensions of the array are encoded along with the data.
If the arrayref is defined, each undefined element represents a dimension that will be encoded
along with the data.

=head2 align

See L<Userp::Type/align>

=cut

has elem_type => ( is => 'ro' );
has dim_type  => ( is => 'ro' );
has dim       => ( is => 'ro' );

sub isa_Array { 1 }

sub _merge_self_into_attrs {
	my ($self, $attrs)= @_;
	$self->next::method($attrs);
	$attrs->{elem_type}= $self->elem_type unless exists $attrs->{elem_type};
	$attrs->{dim_type}= $self->dim_type unless exists $attrs->{dim_type};
	$attrs->{dim}= $self->dim unless exists $attrs->{dim};
}

sub BUILD {
	my $self= shift;
	my $elem_type= $self->elem_type;
	my $elem_align= $elem_type->effective_align;
	my $elem_bits= $elem_type->bitlen;
	# The alignment of the array defaults to the same as the element type
	my $align= defined $self->align? $self->align : $elem_align;
	# Find out if the array has an overal fixed length
	my $bits= undef;
	if (defined $elem_bits and !grep { !defined } @{ $self->dim }) {
		my $mul= 1;
		$mul *= $_ for @{ $self->dim };
		# total bits might be affected by alignment.  Round up length of all elements except last.
		my $mask= (1 << ($elem_align+3)) - 1;
		$bits= $elem_bits + ($mul-1) * (($elem_bits + $mask) & ~$mask);
	}
	$self->_set_effective_align($align);
	$self->_set_bitlen($bits);
}

1;
