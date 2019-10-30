package Userp::PP::ScopeImpl;
use Moo::Role;
use Carp;

# This package contains implementation details for Userp::Scope that are
# only needed if the XS version is not available.

# Each identifier maps to a record of
# [
#   $id,        # index within the identifier table
#   $name,      # the identifier string
#   $prefix_id, # identifier id of a prefix, used to encode smaller
#   $type,      # a type, if one exists with this name
# ]
has _ident_by_id   => ( is => 'rw' );
has _ident_by_val  => ( is => 'rw' );

# simple array of Userp::Type objects
has _type_table    => ( is => 'rw' );

after BUILD => sub {
	my $self= shift;
	my $id= $self->first_ident_id;
	# The Ident table could be quite large, and it needs deep-cloned, so only initialize these
	# and then import the rest from parent lazily.
	my (%by_id, %by_val);
	for (@{ $self->idents }) {
		$by_val{$_}= $by_id{$id}= [ $id, $_, undef, undef ];
		++$id;
	}
	$self->_ident_by_id(\%by_id);
	$self->_ident_by_val(\%by_val);
	
	# The type table is probably smaller, and doesn't need deep-cloned, so just copy it from parent.
	$self->_type_table([
		($self->parent? @{ $self->parent->_type_table } : (undef)), # list is 1-based.
		@{ $self->types }
	]);
	for my $type (@{ $self->types }) {
		my $ident= $type->ident;
		# A record for this identifier should exist, unless it already existed in parent.
		my $rec= $self->_ident_info(undef, $ident)
			or croak "Identifier for type '$ident' does not exist in current scope";
		$rec->[3]= $type;
	}

};

sub _ident_info {
	# ($self, $id, $value)= @_
	my $rec= defined $_[1]? $_[0]{_ident_by_id}{$_[1]} : $_[0]{_ident_by_val}{$_[2]};
	if (!$rec && $_[0]->parent && ($rec= $_[0]->parent->_ident_info($_[1], $_[2]))) {
		$rec= [ @$rec ];
		$_[0]->_ident_by_id->{$rec->[0]}= $rec;
		$_[0]->_ident_by_val->{$rec->[1]}= $rec;
	}
	return $rec;
}

sub ident_id {
	my $rec= $_[0]->_ident_info(undef, $_[1]);
	return $rec? $rec->[0] : undef;
}

sub ident_by_id {
	my $rec= $_[0]->_ident_info($_[1]);
	return $rec->[0] if defined $rec;
	croak "IdentID out of range: $_[1]";
}

sub type_by_id {
	my $t= $_[1] > 0? $_[0]{_type_table}{$_[1]} : undef;
	return $t if defined $t;
	croak "TypeID out of range: $_[1]";
}

sub type_by_ident {
	my $rec= $_[0]->_ident_info($_[1]);
	return $rec? $rec->[3] : undef;
}

1;
