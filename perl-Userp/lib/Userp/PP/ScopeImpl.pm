package Userp::PP::ScopeImpl;
use Moo::Role;
use Carp;

# This package contains implementation details for Userp::Scope that are
# only needed if the XS version is not available.

# Each symbol maps to a record of
# [
#   $id,        # index within the symbol table
#   $name,      # the symbol string
#   $prefix_id, # symbol id of a prefix, used to encode smaller
#   $type,      # a type, if one exists with this name
# ]
# The symbol table needs deep-cloned from parent, so lazy-build each
# entry of the table on demand.
has _symbol_table  => ( is => 'rw', default => sub { [ (undef) x ($_[0]->parent? 1+$_[0]->parent->max_symbol_id : 1 ] } );
has _symbol_id_map => ( is => 'rw', default => sub { +{} } );

# simple array of Userp::Type objects
has _type_table      => ( is => 'rw', default => sub { [ (undef) x ($_[0]->parent? 1+$_[0]->parent->max_type_id : 1 ] } );
has _type_id_by_name => ( is => 'rw' );

sub max_symbol_id {
	return $#{ $_[0]->_symbol_table };
}

sub max_type_id {
	return $#{ $_[0]->_type_table };
}

has _type_any           => ( is => 'rwp' );
has _type_any_public    => ( is => 'rwp' );

sub symbol_by_id {
	my ($self, $id)= @_;
	Userp::Error::Domain->assert_minmax($id, 1, $self->max_symbol_id, 'Symbol ID');
	my $sym= $self->_symbol_table->[$id];
	return defined $sym? $sym : $self->parent? $self->parent->symbol_by_id($id) : undef;
}

sub symbol_id {
	my ($self, $sym, $create)= @_;
	my $id= $self->_symbol_id_map->{$sym};
	$id= $self->parent->symbol_id($sym) if !$id && $self->parent;
	if (!$id && $create) {
		push @{ $self->_symbol_table }, $sym;
		$self->_symbol_id_map->{$sym}= $#{ $self->_symbol_table };
	}
}

sub type_by_id {
	my ($self, $id)= @_;
	Userp::Error::Domain->assert_minmax($id, 1, $self->max_type_id, 'Type ID');
	return $self->_type_table->[$id] || ($self->parent? $self->parent->type_by_id($id) : undef);
}

sub type_by_name {
	my ($self, $name)= @_;
	my $id= $self->_type_id_by_name->{$name};
	return $id? $self->_type_table->[$id] : $self->parent? $self->parent->type_by_name($name) : undef;
}

1;
