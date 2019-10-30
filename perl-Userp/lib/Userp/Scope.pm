package Userp::PP::Scope;
use Moo;
use Carp;

=head1 SYNOPSIS

  $my $scope= Userp::Scope->new(
	id          => $id_num,   # ID of block that declared scope
    parent      => $scope,    # or undef
	block_type  => $type,     # root type of each block using this scope
	anon_blocks => $bool,     # whether blocks in this scope lack IDs
    idents      => \@identifiers,  # array of strings
	types       => \@types,        # array of Userp::Type
  );

=head1 DESCRIPTION

Every userp block is encoded in the context of a Scope.  The scope determines what identifiers
and types are available, and what IDs they get encoded with.  In other words, to decode a block
of userp data you must have an identical Scope to the one used to encode that block.
Scopes can be defined by Metadata blocks, or can be created independent of a Userp stream.

Scope objects are read-only.

=head1 ATTRIBUTES

=head2 id

Each Userp block has an optional ID number.  Metadata blocks always have an ID number, and the
scope created by a metadata block inherits this ID as the Scope ID.

=head2 parent

Reference to a parent C<Userp::Scope> object, or C<undef> if this scope is a root scope.

=head2 block_type

The data type encoded at the root of every block using this scope.  For the highest flexibility,
this can be set to type "Any", in which case the block begins with a type code and can contain
anything you like.  At the opposite extreme, if C<block_type> is "Integer" then each block in
this scope will contain only a single integer.

This can be initialized as either a Type object or ID or identifier name of a type.  After
creation, it will be a reference to a type.

=head2 anon_blocks

Each block can be tagged with an ID, with C<0> indicating an anonymous block.  If all your
blocks will be anonymous, you can prevent needing to encode that C<0> each time by setting
this attribute to true.  Note that anonymous blocks cannot contain nested scopes.

=head2 idents

An arrayref of identifiers (strings).  These identifiers are appended to the identifiers of the
parent scope (if any) to form the string table used when encoding identifiers within blocks in
this scope.

=head2 types

Arrayref of L<Userp::Type> objects.  These types are appended to the types of the parent scope
(if any) to form the type table used when encoding blocks in this scope.  Types can be defined
on the fly in a data block, but only the ones defined in the scope will still be available in
the next block.

=head1 DERIVED ATTRIBUTES

=head2 first_ident_id

The identifier ID of C<< $self->idents->[0] >>.

=head2 total_ident_count

The total number of identifiers in the string table.

=head2 first_type_id

The type ID of C<< $self->types->[0] >>.

=head2 total_type_count

The total number of types in scope (including parent types)

=cut

has id             => ( is => 'ro', required => 1 );
has parent         => ( is => 'ro', required => 1 );
has block_type     => ( is => 'rwp', required => 1 );
has anon_blocks    => ( is => 'ro', required => 1 );
has idents         => ( is => 'ro', required => 1 );
has first_ident_id => ( is => 'rwp' ); # initialized by BUILD
sub total_ident_count { $_[0]->first_ident_id + @{ $_[0]->idents } - 1 }
has types          => ( is => 'ro', required => 1 );
has first_type_id  => ( is => 'rwp' ); # initialized by BUILD
sub total_type_count  { $_[0]->first_type_id + @{ $_[0]->types } - 1 }
has _ident_by_id   => ( is => 'ro', default => sub { {} } );
has _ident_by_val  => ( is => 'ro', default => sub { {} } );
has _type_table    => ( is => 'ro', default => sub { [] } );

sub BUILD {
	my $self= shift;
	my $id= $self->parent? $self->parent->total_ident_count+1 : 1; # list is 1-based
	$self->_set_first_ident_id($id);
	# The Ident table could be quite large, and it needs deep-cloned, so only initialize these
	# and then import the rest from parent lazily.
	for (@{ $self->idents }) {
		$self->_ident_by_val->{$_}=
			$self->_ident_by_id->{$id}=
				[ $id, $_, undef ];
		++$id;
	}
	# The type table is probably smaller, and doesn't need deep-cloned, so just copy it from parent.
	my $types= $self->_type_by_id;
	@$types= $self->parent? @{ $self->parent->_type_table } : (undef); # list is 1-based.
	$self->_set_first_type_id(scalar @$types);
	push @$types, @{ $self->types };
	for my $type (@{ $self->types }) {
		my $ident= $type->ident;
		# A record for this identifier should exist, unless it already existed in parent.
		my $rec= $self->_ident_by_val->{$ident};
		if ($rec) {
			$rec->[3]= $type;
		}
		else {
			my $id= $self->parent->ident_id($ident)
				or croak "Identifier for type $ident does not exist in current scope";
			$self->_ident_by_val->{$ident}=
				$self->_ident_by_id->{$id}= [ $id, $ident, $type ];
		}
	}
}

# Each identifier maps to a record of
# [
#   $id,        # index within the identifier table
#   $name,      # the identifier string
#   $prefix_id, # identifier id of a prefix, used to encode smaller
#   $type,      # a type, if one exists with this name
# ]

sub _ident_info {
	# ($self, $id, $value)= @_
	my $rec= defined $_[1]? $_[0]{_ident_by_id}{$_[1]} : $_[0]{_ident_by_val}{$_[2]};
	if (!$rec && $_[0]->parent && ($rec= $_[0]->parent->_ident_info($_[1], $_[2]))) {
		$rec= [ @$rec ];
		$_[0]{_ident_by_id}{$rec->[0]}= $rec;
		$_[0]{_ident_by_val}{$rec->[1]}= $rec;
	}
	return $rec;
}

=head1 METHODS

=head2 ident_id

  my $id= $scope->ident_id("String");
  # undef is "String" is not in scope

=head2 ident_by_id

  my $string= $scope->ident_by_id($id);
  # dies if $id is out of range

=cut

sub ident_id {
	my $rec= $_[0]->_ident_info(undef, $_[1]);
	return $rec? $rec->[0] : undef;
}

sub ident_by_id {
	my $rec= $_[0]->_ident_info($_[1]);
	return $rec->[0] if defined $rec;
	croak "IdentID out of range: $_[1]";
}

=head2 type_by_id

  my $type= $scope->type_by_id($id);
  # dies if $id out of range

=head2 type_by_ident

  my $type= $scope->type_by_ident("Name");
  # undef if no type by that name

=cut

sub type_by_id {
	my $t= $_[1] > 0? $_[0]{_type_table}{$_[1]} : undef;
	return $t if defined $t;
	croak "TypeID out of range: $_[1]";
}

sub type_by_ident {
	my $rec= $_[0]->_ident_info($_[1]);
	return $rec? $rec->[2] : undef;
}

1;
