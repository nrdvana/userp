package Userp::Type::Record;
use Moo;
extends 'Userp::Type';
use Carp;
use List::Util 'max';
use constant {
	SEQUENCE => -1,
	OPTIONAL => -2,
};

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

=head2 fields

An array of field definitions, each of the form:

=over

=item name

A symbol specifying the field name.

=item type

The type of the field's value.  The type determines how many bytes the field occupies, and
whether it is dynamic-length.

=item placement

One of C<SEQUENCE> (-1), C<OPTIONAL> (-2), or a bit offset from the start of the record.

Byte-offset fields come first (and may overlap) followed by sequential fields followed by
any optional or ad-hoc fields the user decides to include.

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

=head2 adhoc_fields

Boolean.  If enabled (the default) a record can be followed by any number of arbitrary fields
written as a C<< name,value >> pairs, where the name is any Symbol not used by another field
and the value is type C<Any>.

=head2 align

See L<Userp::Type/align>

=head1 OTHER CONSTRUCTOR OPTIONS

The constructor takes attributes directly, but you can also use any of the following
field-defining shortcuts.  (and these shortcuts can be used to extend a previous record type)

=over

=item static_fields

An arrayref of C<< [ $name, $type, $bit_offset ] >>.  Adds or updates a field with static placement.

=item sequential_fields

An arrayref of C<< [ $name, $type ] >>. Adds or updates a field with C<Sequence> placement.

=item optional_fields

An arrayref of C<< [ $name, $type ] >>. Adds or updates a field with C<Optional> placement.

=back

=cut

has fields           => ( is => 'lazy', init_arg => undef );
sub _build_fields { [ @{ $_[0]->_static_fields }, @{ $_[0]->_seq_fields }, @{ $_[0]->_opt_fields } ] }
has static_bits      => ( is => 'ro' );
has adhoc_fields     => ( is => 'ro' );

has _static_fields   => ( is => 'rw' );
has _seq_fields      => ( is => 'rw' );
has _opt_fields      => ( is => 'rw' );
has _field_by_name   => ( is => 'rw' );

sub isa_Record { 1 }

sub get_field {
	$_[0]->_field_by_name->{$_[1]}
}
sub field {
	$_[0]->_field_by_name->{$_[1]} or croak "No field '$_[1]' in Record type '".$_[0]->name."'";
}

has effective_static_bits => ( is => 'rwp' );
has effective_bits        => ( is => 'rwp' );

sub BUILD {
	my ($self, $args)= @_;
	my (@static_f, @seq_f, @opt_f, %by_name);
	my $add_field= sub {
		my ($self, $name, $type, $placement)= @_;
		defined $name && !ref($name) && Userp::Bits::is_valid_symbol($name)
			or croak "Invalid record field name '$name' in record ".$self->name;
		$type && ref($type) && ref($type)->isa('Userp::Type')
			or croak "Type required for field ".$self->name.".$name";
		defined $placement && $placement >= OPTIONAL
			or croak "Invalid placement '$placement' for field ".$self->name.".$name";
		if ($placement >= 0) {
			defined $type->effective_bits
				or croak "Record field ".$self->name.".$name cannot have static placement with dynamic-length type ".$type->name; 
			!defined $self->static_bits || $placement + $type->effective_bits <= $self->static_bits
				or croak "Record field ".$self->name.".$name exceeds static_bits of ".$self->static_bits; 
		}
		if (defined (my $prev= $by_name{$name})) {
			my $pp= $prev->placement;
			if ($pp == $placement) {
				# update the field without moving it
				$prev->{type}= $type;
				return;
			} else {
				# remove it from the list it was in
				my $list= $pp == OPTIONAL? \@opt_f : $pp == SEQUENCE? \@seq_f : \@static_f;
				@$list= grep $_ != $prev, @$list;
			}
		}
		my $list= $placement == OPTIONAL? \@opt_f : $placement == SEQUENCE? \@seq_f : \@static_f;
		my $f= bless { name => $name, type => $type, placement => $placement }, 'Userp::Type::Record::Field';
		push @$list, $f;
		$by_name{$name}= $f;
	};
	my $fl;
	if (defined ($fl= $args->{fields})) {
		for (@$fl) {
			my ($name, $type, $placement)= ref $_ eq 'HASH'? (@{$_}{'name','type','placement'})
				: ( $_->name, $_->type, $_->placement );
			$self->$add_field($name, $type, $placement);
		}
	}
	if (defined ($fl= $args->{static_fields})) {
		$self->$add_field(@$_) for @$fl;
	}
	if (defined ($fl= $args->{sequential_fields})) {
		$self->$add_field($_->[0], $_->[1], SEQUENCE) for @$fl;
	}
	if (defined ($fl= $args->{optional_fields})) {
		$self->$add_field($_->[0], $_->[1], OPTIONAL) for @$fl;
	}
	
	# Record the index of each field within its respective group
	$static_f[$_]{idx}= $_ for 0 .. $#static_f;
	$seq_f[$_]{idx}= $_ for 0 .. $#seq_f;
	$opt_f[$_]{idx}= $_ for 0 .. $#opt_f;
	
	# calculate static_bits
	my $static_bits= $self->static_bits;
	$static_bits= max 0, map $_->placement + $_->type->effective_bits, @static_f
		unless defined $static_bits;
	
	# The alignment of the record is 0 if it has any dynamic fields
	my $align= (@opt_f || $self->adhoc_fields)? 0
		# Else it is the declared alignment
		: defined $self->align? $self->align
		# Else it is the same as the alignment of the first mandatory field
		: @static_f? $static_f[0]->type->effective_align
		: @seq_f? $seq_f[0]->type->effective_align
		# One of the above must be true
		: die "BUG: can't determine effective alignment of record";

	# Find out if the record has an overall fixed length
	my $bits= undef;
	if (!@opt_f && !$self->adhoc_fields) {
		$bits= $static_bits;
		# sequence field is only fixed-length if its alignment is <= the record's own alignment
		# and if it has a constant length
		for (@seq_f) {
			if ($_->type->effective_align <= $align && defined $_->type->bitlen) {
				my $mask= 1 << ($_->type->effective_align+3);
				$bits= ($bits + ($mask - 1)) & ~$mask;
				$bits += $_->type->bitlen;
			} else {
				$bits= undef;
				last;
			}
		}
	}

	$self->_static_fields(\@static_f);
	$self->_seq_fields(\@seq_f);
	$self->_opt_fields(\@opt_f);
	$self->_field_by_name(\%by_name);
	$self->_set_effective_static_bits($static_bits);
	$self->_set_effective_align($align);
	$self->_set_bitlen($bits);
}

1;
