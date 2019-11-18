package Userp::Scope;
use Moo;
use Carp;
use Userp::Error;
use Userp::Type::Any;
use Userp::Type::Integer;
use Userp::Type::Symbol;
use Userp::Type::Choice;
use Userp::Type::Array;
use Userp::Type::Record;

=head1 SYNOPSIS

  my $scope= Userp::Scope->new();
  $scope->add_symbol($_) for @symbols;
  my $Int16= $scope->add_type("Int16", "Integer", { twosc => 16 });
  my $Int16u= $scope->add_type("Int16u", "Integer", { min => 0, max => 0xFFFF });

=head1 DESCRIPTION

Every userp block is encoded in the context of a Scope.  The scope determines what identifiers
and types are available, and what IDs they get encoded with.  In other words, to decode a block
of userp data you must have an identical Scope to the one used to encode that block.
Scopes can be defined by Metadata blocks, or can be created independent of a Userp stream.

Scope objects ought to be immutable, except that it is useful to have a place to store the types
and identifiers as the Scope is being built, so it can be appended until L</final> is set.

=head1 ATTRIBUTES

=over

=item parent

Reference to a parent C<Userp::Scope> object, or C<undef> if this scope is a root scope.
Root scopes automatically contain the following types: Any, Integer, Symbol, Choice, Array, Record.
Scopes with a parent start with all the symbols and types of the parent, and can only be extended.

=item final

Set this to true to prevent any accidental changes to the scope.

=back

=cut

has parent            => ( is => 'ro' );
has final             => ( is => 'rw' );

=head1 SYMBOL TABLE

The symbol table is simply a table of strings.  Each symbol maps to an ID number, starting at 1.

=over

=item symbol_by_id

  my $string= $scope->symbol_by_id($id);

Returns the string value of the C<N>th symbol in the symbol table.
Dies if the ID is out of bounds; ID C<0> is also considered out of bounds.

=item symbol_id

  my $id= $scope->symbol_id("String");

Returns the Symbol ID (if any) of the given string.  Returns C<undef> for non-existent symbols.

=item add_symbol

  my $id= $scope->add_symbol($symbol);

Add a symbol to the symbol table.  This returns the existing ID if the symbol already existed.

=item max_symbol_id

The highest defined symbol ID in the symbol table.

=back

=cut

# lazy-populate the table
has _symbol_table_ofs => ( is => 'rw' );
has _symbol_table     => ( is => 'rw' );
has _symbol_id_map    => ( is => 'rw' );
has max_symbol_id     => ( is => 'rwp' );

sub symbol_by_id {
	my ($self, $id)= @_;
	return $self->parent->symbol_by_id($id) if $id < $self->_symbol_table_ofs;
	Userp::Error::Domain->assert_minmax($id, 1, $self->max_symbol_id, 'Symbol ID');
	return $self->_symbol_table->[$id - $self->_symbol_table_ofs];
}

sub symbol_id {
	my ($self, $sym, $create)= @_;
	return $self->_symbol_id_map->{$sym} if $self->_symbol_id_map;
	return $self->parent->symbol_id($sym) if $self->parent;
	return undef;
}

sub add_symbol {
	my ($self, $value)= @_;
	my $id= $self->symbol_id($value);
	return $id if $id;
	Userp::Error::Readonly->throw({ message => 'Cannot alter symbol table of a finalized scope' })
		if $self->final;
	my $st= $self->{_symbol_table} ||= [];
	my $s_by_val= $self->{_symbol_id_map} ||= {};
	$id= $self->_symbol_table_ofs + @$st;
	push @$st, $value;
	$s_by_val->{$value}= $id;
	$self->_set_max_symbol_id($id);
	return $id;
}

=head1 TYPE TABLE

Like the symbol table, the type table is a simple array of types.  Each type gets a distinct ID
starting at 1, but also gets a name to help identify it.  Duplicate names are permitted (but
discouraged); the most recent takes priority in name lookups.

=over

=item type_by_id

  my $type= $scope->type_by_id($id);

Returns the C<N>th L<Userp::Type> object in the type table.  
Dies if the ID is out of bounds; ID C<0> is also considered out of bounds.

=item type_by_name

  my $type= $scope->type_by_name("Name");

Returns the L<Userp::Type> object with the given name, or C<undef> if no type has that name.

=item add_type

  my $type= $scope->add_type( $name, $spec );
  my $type= $scope->add_type( $name, $base_type, \%attributes, \%meta );

Create a new Userp::Type object.  The first form parses a type specification in text notation.
The second one subclasses another type by applying attributes and optional metadata.

=item max_type_id

The highest defined type in the type table.

=back

=cut

# simple array of Userp::Type objects
has _type_table_ofs   => ( is => 'rw' );
has _type_table       => ( is => 'rw' );
has _type_id_by_name  => ( is => 'rw' );
has max_type_id       => ( is => 'rwp' );

sub type_by_id {
	my ($self, $id)= @_;
	return $self->parent->type_by_id($id) if $id < $self->_type_table_ofs;
	Userp::Error::Domain->assert_minmax($id, 1, $self->max_type_id, 'Type ID');
	return $self->_type_table->[$id - $self->_type_table_ofs];
}

sub type_by_name {
	my ($self, $name)= @_;
	my $id= $self->_type_id_by_name? $self->_type_id_by_name->{$name} : undef;
	return $self->_type_table->[$id - $self->_type_table_ofs] if $id;
	return $self->parent->type_by_name($name) if $self->parent;
	return undef;
}

sub add_type {
	my ($self, $name, $base, $attrs, $meta)= @_;
	Userp::Error::Readonly->throw({ message => 'Cannot alter type table of a finalized scope' })
		if $self->final;
	my $tt= $self->{_type_table} ||= [];
	my $t_by_name= $self->{_type_id_by_name} ||= {};
	my $type_id= $self->_type_table_ofs + @$tt;
	$base= $self->type_by_name($base) || Userp::Error::NotInScope->throw({ message => "No type '$base' in current scope" })
		unless ref $base;
	my $type= $base->subtype(($attrs? %$attrs : ()), id => $type_id, name => $name);
	$type->set_meta($meta) if defined $meta;
	$self->add_symbol($name);
	push @$tt, $type;
	$t_by_name->{$name}= $type_id;
	$self->_set_max_type_id($type_id);
	return $type;
}

=head2 Type Shortcuts

Several types exist in every scope.  These accessors provide slightly more efficient access
to them than L</type_by_name>.

=over

=item type_Any

The Choice of any defined type.

=item type_Integer

The type of all Integers

=item type_Symbol

The type of all Symbols

=item type_Choice

The Choice of nothing. (mainly used as a parent for derived types)

=item type_Array

A single or multi-dimensional array of any element data type.

=item type_Record

The type of a record with arbitrary keys and value types.

=back

=cut

sub type_Any     { $_[0]{type_Any}     ||= shift->type_by_id(1) }
sub type_Integer { $_[0]{type_Integer} ||= shift->type_by_id(2) }
sub type_Symbol  { $_[0]{type_Symbol}  ||= shift->type_by_id(3) }
sub type_Choice  { $_[0]{type_Choice}  ||= shift->type_by_id(4) }
sub type_Array   { $_[0]{type_Array}   ||= shift->type_by_id(5) }
sub type_Record  { $_[0]{type_Record}  ||= shift->type_by_id(6) }

sub BUILD {
	my $self= shift;
	# If there is a parent, extend its type table
	# else create a fresh minimal type table
	if ($self->parent) {
		# Tables are an extension of parent's
		$self->_symbol_table_ofs($self->parent->max_symbol_id+1);
		$self->_type_table_ofs($self->parent->max_type_id+1);
	} else {
		$self->_symbol_table_ofs(0);
		$self->_symbol_table([ undef ]);
		$self->_type_table_ofs(0);
		my $tAny= Userp::Type::Any->new(id => 1, name => 'Any');
		my $tSym= Userp::Type::Any->new(id => 3, name => 'Symbol');
		$self->_type_table([
			undef,
			$tAny,
			Userp::Type::Integer->new(id => 2, name => 'Integer'),
			$tSym,
			Userp::Type::Choice->new(
				id => 4,
				name => 'Choice',
				options => [],
			),
			Userp::Type::Array->new(
				id => 5,
				name => 'Array',
				elem_type => $tAny,
			),
			Userp::Type::Record->new(
				id => 6,
				name => 'Record',
				fields => [],
				adhoc => 1,
				adhoc_name_type => $tSym,
				adhoc_value_type => $tAny,
			),
		]);
		$self->_type_id_by_name({
			Any => 1, Integer => 2, Symbol => 3, Choice => 4, Array => 5, Record => 6
		});
	}
}

#=head2 add_type_from_spec
#
#This parses Userp Data Notation that describes a type.  A specification for a type consists of
#a type name (a Symbol) Type being extended (a Type) Attributes to override (a Record) and
#optional Metadata (a Record).
#
#Example:
#
#  MyRecord Record (align=8 fields=((x !Float32) (y !Float32) (z !Float32))) ()
#
#=cut
#
#sub add_type_from_spec {
#	
#}

1;
