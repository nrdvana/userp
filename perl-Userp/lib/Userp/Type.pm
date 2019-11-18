package Userp::Type;
use Moo 2;

=head1 ATTRIBUTES

=head2 name

The name of this type.  Must be a valid identifier defined in the current Scope.

=head2 id

The numeric index of this type within the scope that defines it.

=head2 spec

The text specification for this type.

=head2 align

The alignment for values of this type, in power-of-2 bits.  0 means bit-aligned, 3 means
byte-aligned, 5 means DWORD aligned, and so on.  Alignment is measured from the start of a
block's data.

Note that any type starting with a variable-length quantity will be at least byte-aligned.

=head2 metadata

Every type can have generic key/value metadata associated with it.  The metadata has no effect
on the encoding of the data, but applications might use it to change how they interpret the
data.  For the initial version of the library, metadata is only encoded as generic key/value
records, though later versions might allow more specific types in the metadata values.

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
has spec       => ( is => 'lazy' );
has align      => ( is => 'ro', default => 3 );
has metadata   => ( is => 'ro' );

sub isa_any { 0 }
sub isa_int { 0 }
sub isa_sym { 0 }
sub isa_chc { 0 }
sub isa_ary { 0 }
sub isa_rec { 0 }

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

=head2 subtype

  my $type2= $type1->subtype( %attrs );

Returns a new type instance with the specified attributes altered.  In some cases, the given
attributes overwrite the current object's attributes, and in other cases the values get
combined in a way that feels like "subclassing" the type.  For instance, L<Userp::Type::Integer>
C<names> are cumulative, L<Userp::Type::Record> fields are cumulative, and so on.  Any
conflicting attributes are resolved in favor of the new value.

C<%attrs> must always include C<id>, C<public_id>, and C<name>, even if they are C<undef>.

=cut

sub subtype {
	my ($self, %attrs)= @_ == 2? ($_[0], %{$_[0]}) : @_;
	my $class= ref($self) || $self;
	$self->_merge_self_into_attrs(\%attrs) if ref $self;
	return $class->new(\%attrs);
}

# This gets overridden in subclasses
sub _merge_self_into_attrs {
	my ($self, $attrs)= @_;
	$attrs->{align}= $self->align unless defined $attrs->{align};
	if ($self->metadata && %{$self->metadata}) {
		my %merged= ( %{$self->metadata}, ($attrs->{metadata}? %{$attrs->{metadata}} : ()) );
		$attrs->{metadata}= \%merged;
	}
}

=head2 has_member_type

  $bool= $type->has_member_type($other_type);

This only returns true on Union types when the C<$other_type> is a member or sub-member.

=cut

sub has_member_type { 0 }

1;
