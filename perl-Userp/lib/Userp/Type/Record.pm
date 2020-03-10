package Userp::Type::Record;
use Moo;
extends 'Userp::Type';
use Carp;
use List::Util 'max';
use constant {
	ALWAYS => -1,
	OFTEN  => -2,
	SELDOM => -3
};
our @EXPORT_OK= qw( ALWAYS OFTEN SELDOM );
use Exporter 'import';

=head1 DESCRIPTION

A record is a sequence where the elements can each have their own delcared type, and each
element is named with a unique identifier.  Record encoding handles four key types of field:
static-placement fields that occur at a specific byte offset, sequential fields that are
always present and packed end-to-end, optional fields that may or may not be present, and
ad-hoc fields that are not pre-declared and are encoded as key/value pairs.

The order of sequential and optional fields is part of the type definition, but the Userp
library allows out-of-order access.

The Record type serves a dual role as a way to match low-level encodings like C-language
structs but also handle looser key/value pairs like JavaScript objects.
Records are not a generic key/value dictionary though; field names must adhere to a
restricted set of Unicode and cannot be generic data.

=head1 ATTRIBUTES

=head2 name

Name of the type

=head2 parent

Optional type which this record inherits fields from.

=head2 fields

An array of field definitions, each of the form:

=over

See L<DECLARING FIELDS> for additional details about shortcuts when defining fields and
details of how fields of the parent are merged.

=item name

A symbol specifying the field name.

=item type

The type of the field's value.  The type determines how many bytes the field occupies, and
whether it is dynamic-length.  Must be a type object, not a type name.

=item placement

=over

=item Non-negative bit offset

The field is located at this bit offset within the static area at the start of the record.
The type must be a static length.  If the type has an alignment, this record must have at least
that much alignment and this bit offset must land on that alignment.

=item C<ALWAYS> (-1)

The field is always encoded, following any static area.

=item C<OFTEN> (-2)

The presence of the field will be indicated by a bit before the start of the record.
If the bit is set, the encoded field will follow the C<ALWAYS> fields.

=item C<SELDOM> (-3)

The presence of the field will be indicated by whether its field ID occurs in a list
of IDs before the start of the record.  If present, its encoded value will follow the
C<OFTEN> fields.

=back

=back

=cut

{
	package Userp::Type::Record::Field;
	sub name { $_[0]{name} }
	sub type { $_[0]{type} }
	sub placement { $_[0]{placement} }
	sub idx  { $_[0]{idx} }
}

=head2 static_bits

The number of bits reserved for static fields.  This is only needed if you have a record with
extra padding (before the sequential or optional fields) in which no static fields are defined
yet.  If set, it is an error to define static fields beyond the end of the static area.

=head2 extra_field_type

The type to use for all extra fields.  If set to a non-NULL type, this allows the record to
include additional un-declared fields using any name in the current symbol table.
The default is C<Userp.Any>.

=head2 align

Set the alignment of the first field value in the record, or of the static area if present.
The bytes defining which fields are present in the record come before the point of alignment.

=cut

has fields           => ( is => 'rwp', init_arg => undef );
has static_bits      => ( is => 'ro' );
has extra_field_type => ( is => 'ro' );
has align            => ( is => 'ro' );

has _static_count   => ( is => 'rw' );
sub _always_ofs { shift->_static_count }
has _always_count   => ( is => 'rw' );
sub _often_ofs  { $_[0]->_static_count + $_[0]->_always_count }
has _often_count    => ( is => 'rw' );
sub _seldom_ofs { $_[0]->_static_count + $_[0]->_always_count + $_[0]->_often_count }
has _seldom_count   => ( is => 'rw' );

has _field_by_name   => ( is => 'rw' );

sub isa_Record { 1 }
sub base_type { 'Record' }

sub get_field {
	$_[0]->_field_by_name->{$_[1]}
}
sub field {
	$_[0]->_field_by_name->{$_[1]} or croak "No field '$_[1]' in Record type '".$_[0]->name."'";
}

has effective_align       => ( is => 'rwp' );
has effective_static_bits => ( is => 'rwp' );

=head1 DECLARING FIELDS

The list of fields passed to the constructor can be any of the following forms:

  fields => [
    # full specification
    { name => $name, type => $type, placement => OFTEN },
	
    # partial specification inherits type and placement from previous
	{ name => $name },
	
	# arrayref
	[ $name, $type, $placement ],
	
	# or just a name
	$name
  ]
  
These will be coerced into an arrayref of C<Userp::Type::Record::Field> objects and sorted by
placement such that all static fields are listed first, then C<ALWAYS>, then C<OFTEN>,
then C<SELDOM>.

When a parent is given, the fields are inherited.  New fields with the same name as ones in the
parent will replace the definition of those in the parent, but use the type or placement from
the parent if not re-specified.  New fields are appended to those in the parent but then sorted
by placement, as a normal field list would be.

=cut

sub BUILD {
	my ($self, $args)= @_;
	my (@static_f, @always_f, @often_f, @seldom_f, %by_name);
	my $placement_list= sub {
		$_[0] >= 0? \@static_f
		: $_[0] == ALWAYS? \@always_f
		: $_[0] == OFTEN? \@often_f
		: $_[0] == SELDOM? \@seldom_f
		: die "Bug";
	};
	my $align= $self->align;
	my $add_field= sub {
		my ($self, $name, $type, $placement)= @_;
		defined $name && !ref($name) && Userp::Bits::is_valid_symbol($name)
			or croak "Invalid record field name '$name' in record ".$self->name;
		$type && ref($type) && ref($type)->isa('Userp::Type')
			or croak "Type required for field ".$self->name.".$name";
		defined $placement && $placement >= SELDOM
			or croak "Invalid placement '$placement' for field ".$self->name.".$name";
		if ($placement >= 0) {
			defined $type->bitlen
				or croak "Record field ".$self->name.".$name cannot have static placement with dynamic-length type ".$type->name; 
			!defined $self->static_bits || $placement + $type->bitlen <= $self->static_bits
				or croak "Record field ".$self->name.".$name exceeds static_bits of ".$self->static_bits; 
			if (defined $type->align) {
				Userp::Bits::roundup_bits_to_alignment($placement, $type->align) == $placement
					or croak "Record field ".$self->name.".$name placement must be aligned to 2**".($type->align+3)." bits";
				$align= $type->align if !defined $align or $align < $type->align;
			}
		}
		if (defined (my $prev= $by_name{$name})) {
			my $pp= $prev->{placement};
			if ($pp == $placement) {
				# update the field without moving it
				$prev->{type}= $type;
				return;
			} else {
				# remove it from the list it was in
				my $list= $placement_list->($pp);
				@$list= grep $_ != $prev, @$list;
			}
		}
		my $list= $placement_list->($placement);
		my $f= { name => $name, type => $type, placement => $placement };
		push @$list, $f;
		$by_name{$name}= $f;
	};
	if ($self->parent) {
		for (@{ $self->parent->fields }) {
			$add_field->($_->name, $_->type, $_->placement);
		}
	}
	if (defined $args->{fields}) {
		my $prev_placement= ALWAYS;
		require Userp::RootTypes;
		my $prev_type= Userp::RootTypes::type_Any();
		for (@{ $args->{fields} }) {
			my ($name, $type, $placement)= ref $_ eq 'HASH'? (@{$_}{'name','type','placement'})
				: ref $_ eq 'ARRAY'? (@$_)
				: !ref $_? ($_)
				: ref($_)->can('placement')? ( $_->name, $_->type, $_->placement )
				: croak "Unknown field specification $_";
			$type= $prev_type unless defined $type;
			ref $type or croak "field type must be a type reference, not a name";
			$placement= $prev_placement unless defined $placement;
			$self->$add_field($name, $type, $placement);
			$prev_type= $type;
			$prev_placement= $placement;
		}
	}
	
	# calculate static_bits
	my $static_bits= $self->static_bits;
	$static_bits= max 0, map $_->{placement} + $_->{type}->bitlen, @static_f
		unless defined $static_bits;
	
	# If a record has dynamic fields, then the alignment (which applies to the static bits or first
	# field of ALWAYS placement) ends up at least byte-aligned (0).
	if (@often_f || @seldom_f || $self->extra_field_type) {
		$align= 0 unless defined $align and $align > 0;
	}
	# If a record does not have static bits and the first ALWAYS field has higher alignment,
	# then that is the effective alignment.
	elsif (!@static_f && @always_f) {
		my $first_align= $always_f[0]{type}->effective_align;
		$align= $first_align if !defined $align or $align < $first_align;
	}
	
	# Find out if the record has an overall fixed length
	my $bits= undef;
	unless (@often_f || @seldom_f || $self->extra_field_type) {
		$bits= $static_bits;
		# ALWAYS field is only fixed-length if its alignment is <= the record's own alignment
		# and if it has a constant length
		for (@always_f) {
			if ($_->{type}->effective_align <= $align && defined $_->{type}->bitlen) {
				$bits= Userp::Bits::roundup_bits_to_alignment($bits, $_->{type}->effective_align)
					+ $_->{type}->bitlen;
			} else {
				$bits= undef;
				last;
			}
		}
	}

	# Combine the lists
	my @fields= ( @static_f, @always_f, @often_f, @seldom_f );
	# Record the index of each field
	$fields[$_]{idx}= $_ for 0 .. $#fields;
	# Then bless them into objects
	bless $_, 'Userp::Type::Record::Field' for @fields;
	# Make them read-only and blessed as objects
	Const::Fast::const my $f => \@fields;
	
	$self->_set_fields($f);
	$self->_field_by_name(\%by_name);
	$self->_set_effective_static_bits($static_bits);
	$self->_set_effective_align($align);
	$self->_set_bitlen($bits);
}

sub _register_symbols {
	my ($self, $scope)= @_;
	$scope->add_symbols(map $_->name, @{$self->fields});
	$self->next::method($scope);
}

1;
