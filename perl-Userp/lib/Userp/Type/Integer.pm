package Userp::Type::Integer;
use Carp;
use Userp::Error;
use Userp::Bits;
use Moo;
use Const::Fast;
extends 'Userp::Type';

=head1 DESCRIPTION

The Integer type can represent an infinite range of integers, a half-infinite range, or a
finite range.  It can also specify a number of bits to allow for 2's complement encoding.

An Integer type can also have named values.  When encoding, you can specify a symbol value
and it will encode the related integer.  During decoding, the value can be read as either
a symbol or an integer, though it will decode as an integer by default.

(For a true Enumeration, with no user-visible integer, just add Symbol values to a Choice type.)

=head1 ATTRIBUTES

=head2 min

If specified, the integer will be encoded as an unsigned offset from this value.

Note that this is specific to the encoding, and not meant as a general-purpose type constraint.

=head2 max

If C<max> is specified and C<min> is not, the integer will be encoded as an unsigned offset
in the negative direction from this value.  If both C<min> and C<max> are specified, the value
is encoded increasing from C<min> and C<max> simply limits the overall domain of the integer
type.  (so that it may efficiently be inlined into a Choice type)

Note that this is specific to the encoding, and not meant as a general-purpose type constraint.

=head2 bits

If specified, this is the number of bits in which the integer will be encoded.  If C<min> and
C<max> are not specified, the value is encoded as a signed 2's complement, and sign-extended
when decoded.  If C<min> or C<max> are specified, the encoding will be unsigned, but still
occupy this many bits rather than being a variable-sized integer or the minimum bits to encode
C<< max - min + 1 >>.

=head2 names

A hashref of C<< { $symbol => $value } >>.  There may be more than one name for a value, but
this is discouraged.

When passed to the constructor, this may be given as a short-hand arrayref:

  [ [ $symbol => $value ], $symbol2, $symbol3, ... ]

Where an arrayref element declares both a name and value, and any scalar element declares
the name of the previous value plus one.  If the first element is a scalar, its value is
assumed to be zero.

Names passed to the constructor are merged with those of the parent type, with the new
value taking precedence if any name conflicts.  It is not possible to un-declare a named
value from the parent.

All declared values must be consistent with C<min>, C<max>, and C<bits>.

=head2 align

Set the alignment of the encoded value.  Any variable-lengh integer must be at least
byte-aligned ( C<< align = 0 >> ).  All other integer type default to bit alignment.

The pre-defined integer types C<Userp.Int32>, C<Userp.Int64>, etc come with an alignment
matching the C compiler defaults for that type.

=cut

has min   => ( is => 'ro' );
has max   => ( is => 'ro' );
has bits  => ( is => 'ro' );
has names => ( is => 'ro', coerce => \&_coerce_const_name_set );
has align => ( is => 'ro' );

sub isa_Integer { 1 }
sub base_type { 'Integer' }

has effective_min   => ( is => 'rwp' );
has effective_max   => ( is => 'rwp' );
sub effective_bits { shift->bitlen }
has effective_align => ( is => 'rwp' );
has name_count      => ( is => 'lazy' );

has _name_by_val => ( is => 'lazy' );
has _val_by_name => ( is => 'lazy' );

sub _coerce_const_name_set {
	my $names= shift;
	if (ref $names eq 'ARRAY') {
		my $v= -1;
		Const::Fast::const my %names,
			map +(ref $_ eq 'ARRAY'? ( $_->[0], ($v=$_->[1]) )
				: ref $_ eq 'HASH'? %$_
				: ( $_ => ++$v )),
				@$names;
		$names= \%names;
	}
	elsif (ref $names eq 'HASH') {
		$names= undef unless keys %$names;
		Const::Fast::const $names, $names if $names;
	}
	elsif (defined $names) {
		croak "Unknown format for Integer->names: \"$names\"";
	}
	return $names;
}

sub _build_name_count {
	$_[0]->names? scalar keys %{$_[0]->names} : 0;
}

sub _build__name_by_val {
	{ reverse %{ shift->effective_names } }
}

sub _merge_self_into_attrs {
	my ($self, $attrs)= @_;
	$self->next::method($attrs);
	for (qw( min max bits )) {
		$attrs->{$_}= $self->$_ if !defined $attrs->{$_} && defined $self->$_;
	}
	# names needs to be merged, with new names overriding the same in parent.
	if ($self->names) {
		# If parent has names and child doesn't, just use parent's names
		$attrs->{names}= !$attrs->{names}? $self->names
			# If names given as hashref, merge them with parent.  Const will be applied by coerce
			: ref $attrs->{names} eq 'HASH'? { %{ $self->names }, %{$attrs->{names}} }
			# If names given as arrayref, use undocumented feature that an element of the
			# array may be a hashref; Result will get merged and const-ified in one shot.
			: ref $attrs->{names} eq 'ARRAY'? [ $self->names, @{$attrs->{names}} ]
			: croak "Unknown format for Integer->names: \"$attrs->{names}\"";
	}
}

sub BUILD {
	my $self= shift;
	my ($min, $max, $bits, $align)= ($self->min, $self->max, $self->bits, $self->align);
	# If bits or min+max are set, the integer is fixed-length
	if (defined $bits) {
		# Signed if min and max are not set, unsigned otherwise.
		if (!defined $min && !defined $max) {
			($min, $max)= Userp::Bits::twos_minmax($bits);
		}
		else {
			my $max_unsigned= Userp::Bits::unsigned_max($bits);
			if (defined $min) {
				if (defined $max) {
					Userp::Error::Domain->assert_minmax($min, $max, $min+$max_unsigned, 'Integer max');
				} else {
					$max= $min + $max_unsigned;
				}
			}
			elsif (defined $max) {
				$min= $max - $max_unsigned;
			}
		}
	}
	elsif (defined $min && defined $max) {
		# If min and max are both defined, bits is derived from the largest value that can be encoded
		Userp::Error::Domain->assert_minmax($max, $min, undef, 'Integer max');
		$bits= Userp::Bits::bitlen($max-$min);
	}
	else {
		# variable-length integers are always byte-aligned or higher
		$align= 0 unless defined $align && $align > 0;
	}
	$self->_set_effective_min($min);
	$self->_set_effective_max($max);
	$self->_set_effective_align(defined $align? $align : -3);
	$self->_set_bitlen($bits);
}

sub _get_definition_attributes {
	my ($self, $attrs)= @_;
	if (my $parent= $self->parent) {
		$attrs->{min}= $self->min if Userp::Bits::_deep_cmp($self->min, $parent->min);
		$attrs->{max}= $self->max if Userp::Bits::_deep_cmp($self->max, $parent->max);
		$attrs->{bits}= $self->bits if Userp::Bits::_deep_cmp($self->bits, $parent->bits);
		# Names declared in this type get overlaid on parent.
		# Make a list of every name in this type that differs or doesn't exist in parent.
		if (Userp::Bits::_deep_cmp($self->names, $parent->names)) {
			my $parent_names= $parent->names;
			my %diff;
			for (keys %{ $self->names }) {
				my $v= $self->names->{$_};
				$diff{$_}= $v if !exists $parent_names->{$_} || ($parent_names->{$_} ne $v);
			}
			$attrs->{names}= \%diff;
		}
	}
}

sub _register_symbols {
	my ($self, $scope)= @_;
	$scope->add_symbols(keys %{ $self->names })
		if $self->names;
	$self->next::method($scope);
}

1;
