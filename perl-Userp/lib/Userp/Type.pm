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

=head2 bitlen

A number of bits used to encode this type, or C<undef> if the encoding has a variable length.

=cut

has name       => ( is => 'ro' );
has scope_idx  => ( is => 'rwp' );
has table_idx  => ( is => 'rwp' );
has spec       => ( is => 'lazy' );
sub align         { 0 }
has metadata   => ( is => 'ro' );

sub effective_align { 0 }
has bitlen          => ( is => 'rwp' );

sub isa_Any     { 0 }
sub isa_Integer { 0 }
sub isa_Symbol  { 0 }
sub isa_Choice  { 0 }
sub isa_Array   { 0 }
sub isa_Record  { 0 }

=head1 METHODS

=head2 subtype

  my $type2= $type1->subtype( %attrs );

Returns a new type instance with the specified attributes altered.  New attribute values
replace old ones of the same name, but there are also some keys that can be given that are
not actual attributes (like C<add_options> or C<add_fields>) that augment the value of an
attribute from the parent.

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
	if ($self->metadata && %{$self->metadata}) {
		my %merged= ( %{$self->metadata}, ($attrs->{metadata}? %{$attrs->{metadata}} : ()) );
		$attrs->{metadata}= \%merged;
	}
}

sub _register_symbols {
	my ($self, $scope)= @_;
	$scope->add_symbols($self->name) if defined $self->name;
	if ($self->metadata) {
		$scope->add_symbols(keys %{ $self->metadata });
		for (values %{ $self->metadata }) {
			die "Unimplemented: need to find all Symbols in nested metadata"
				if ref $_;
		}
	}
}

=head2 has_member_type

  $bool= $type->has_member_type($other_type);

This only returns true on Union types when the C<$other_type> is a member or sub-member.

=cut

sub has_member_type { 0 }

1;
