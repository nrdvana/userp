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
has align     => ( is => 'ro' );

has effective_align => ( is => 'rwp' );

sub isa_Array { 1 }

sub _merge_self_into_attrs {
	my ($self, $attrs)= @_;
	$self->next::method($attrs);
	$attrs->{elem_type}= $self->elem_type unless exists $attrs->{elem_type};
	$attrs->{dim_type}= $self->dim_type unless exists $attrs->{dim_type};
	$attrs->{dim}= $self->dim unless exists $attrs->{dim};
	$attrs->{align}= $self->align unless exists $attrs->{align};
}

sub BUILD {
	my $self= shift;
	my $align= $self->align;
	my $elem_count;
	if ($self->dim && !grep { !defined } @{ $self->dim }) {
		$elem_count= 1;
		$elem_count *= $_ for @{ $self->dim };
	}
	my $bits;
	if (defined $elem_count && $elem_count == 0) {
		$bits= 0;
		$align= $self->elem_type? -3 : 0;
	}
	elsif (my $elem_type= $self->elem_type) {
		my $elem_align= $elem_type->effective_align;
		my $elem_bits= $elem_type->bitlen;
		# The alignment of the array must be at least as big as the element alignment
		$align= $elem_align if !defined $align || $align < $elem_align;
		# Find out if the array has an overal fixed length
		if (defined $elem_bits and defined $elem_count) {
			# total bits might be affected by alignment.  Round up length of all elements except last.
			my $mask= (1 << ($elem_align+3)) - 1;
			$bits= $elem_count? $elem_bits + ($elem_count-1) * (($elem_bits + $mask) & ~$mask) : 0;
		}
		else {
			# Alignment of dynamic array must be at least 0
			$align= 0 if $align < 0;
		}
	}
	else {
		# Alignment of dynamic elem type array must be at least 0
		$align= 0 if !defined $align or $align < 0;
	}
	$self->_set_effective_align($align);
	$self->_set_bitlen($bits);
}

1;
