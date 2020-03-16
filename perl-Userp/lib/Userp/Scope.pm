package Userp::Scope;
use Moo;
use Carp;
use Userp::Error;
use Userp::Type::Any;
use Userp::Type::Symbol;
use Userp::Type::Type;
use Userp::Type::Integer;
use Userp::Type::Choice;
use Userp::Type::Array;
use Userp::Type::Record;
use Userp::RootTypes qw( type_Any type_Symbol type_Type type_Int type_Array type_Record );

=head1 SYNOPSIS

  my $scope= Userp::Scope->new();
  $scope->add_symbols($_);
  my $Int16= $scope->define_type("Int16", "Integer", { bits => 16 });
  my $Int16u= $scope->define_type("Int16u", "Integer", { min => 0, max => 0xFFFF });

=head1 DESCRIPTION

Every userp block is encoded in the context of a Scope.  The scope determines what identifiers
and types are available, and what IDs they get encoded with.  In other words, to decode a block
of userp data you must have an identical Scope to the one used to encode that block.
Scopes can be defined by Metadata blocks, or can be created independent of a Userp stream.

Scope objects ought to be immutable, but it is useful to be able to begin encoding the data
before all the symbols have been declared, so this implementation allows a Scope to be appended
until L</final> is set.

=head1 ATTRIBUTES

=over

=item parent

Reference to a parent C<Userp::Scope> object, or C<undef> if this scope is a root scope.
Root scopes automatically contain the following types: Any, Integer, Symbol, Choice, Array, Record.
Scopes with a parent start with all the symbols and types of the parent, and can only be extended.

=item scope_stack

Flattened arrayref of scopes from root to current.  (Root is C<< ->[0] >>, current is C<< ->[-1] >>)

=item scope_idx

Index of this scope within L</scope_stack>.

=item final

Set this to true to prevent any accidental changes to the scope.

=back

=cut

has parent            => ( is => 'ro' );
has final             => ( is => 'rw' );

has scope_stack       => ( is => 'lazy' );
has scope_idx         => ( is => 'lazy' );

sub _build_scope_idx {
	$#{ shift->scope_stack }
}

sub _build_scope_stack {
	my $p= shift;
	my @stack= ( $p );
	while ($p= $p->parent) {
		unshift @stack, $p;
	}
	Scalar::Util::weaken($stack[-1]); # ref to self
	\@stack;
}

=head1 SYMBOL TABLE

The symbol table is simply an array of strings.  It conceptually inherits the parent's symbols,
but the lists are kept separate.  Add symbols to this table with L</add_symbol>, and find them
in this or a parent table using L</find_symbol>.  Retrieve an element of the table with
L</get_symbol>.

=over

=item symbol_count

The number of symbols in this table (not including parent scopes).

=item get_symbol

  my $string= $scope->get_symbol($N);

Returns the string value of the C<N>th symbol in the symbol table.
Dies if the index is out of bounds.

=item find_symbol

  my ($scope_idx, $sym_idx)= $scope->find_symbol("String", $bool_add);

Find a symbol by its string value.  Returns a pair of the scope index (according to scope_stack)
and symbol index.  Returns an empty list if not found.  If the second argument is true, the
symbol will be added if not found, and the new index returned.

=item add_symbols

  my $num_added= $scope->add_symbols($symbol, ...);

Add one or more symbols to the symbol table.  Returns the number that did not already exist
in this or a parent scope.

=back

=cut

has _symbols          => ( is => 'rw', default => sub { [] } );
has _sym_by_name      => ( is => 'rw', default => sub { +{} } );

sub symbol_count {
	scalar @{ shift->_symbols };
}

sub get_symbol {
	$_[0]->_symbols->[$_[1]] // croak "Symbol index out of bounds: $_[1]";
}

sub find_symbol {
	my ($self, $sym, $add)= @_;
	my $scope= $self;
	do {
		my $sym_i= $scope->_sym_by_name->{$sym};
		return ($scope->scope_idx, $sym_i) if defined $sym_i;
		$scope= $scope->parent;
	} while $scope;
	if ($add) {
		Userp::Error::Readonly->throw({ message => 'Cannot alter symbol table of a finalized scope' })
			if $self->final;
		push @{ $self->_symbols }, $sym;
		my $sym_i= $self->_sym_by_name->{$sym}= $#{ $self->_symbols } || '0E0';
		return ($self->scope_idx, $sym_i);
	}
	return;
}

sub add_symbols {
	my $self= shift;
	Userp::Error::Readonly->throw({ message => 'Cannot alter symbol table of a finalized scope' })
		if $self->final;
	my $syms= $self->_symbols;
	my $by_name= $self->_sym_by_name;
	my $parent= $self->parent;
	my $start_n= @$syms;
	for (@_) {
		if (!defined $by_name->{$_} and (!$parent or !$parent->find_symbol($_))) {
			push @$syms, $_;
			$by_name->{$_}= $#$syms || '0E0';
		}
	}
	return @$syms - $start_n;
}

=head1 TYPE TABLE

Like the symbol table, the type table is a simple array of types.  Each type is represented by
a L<Userp::Type> object.  Types can be identified by names, but the names are not required, and
the names are permitted to overlap with names in a parent Scope, so use the reference
rather than the name when possible.  Names beginning with C<'Userp.'> are reserved for the
protocol specification.  "Java-style" reverse Internet domain names are suggested, but not
required.

=over

=item type_count

The number of types defined locally in this scope's table.  (not inherited)

=item get_type

  my $type= $scope->get_type($N);

Returns the C<N>th L<Userp::Type> object in the type table. Dies if the index is out of bounds.

=item find_type

  my $type= $scope->find_type("Name");

Returns the L<Userp::Type> object with the given name, or C<undef> if no type has that name.
To find the index of the type within the table, see L<Userp::Type/table_idx>, and to find the
index of the scope where it was defined, see L<Userp::Type/scope_idx>.

=item define_type

  my $type= $scope->define_type( $name, $base_type, \%attributes, \%meta );

Create a new Userp::Type object.  C<$name> may be C<undef> for an anonymous type.
C<$base_type> myst be a reference or name of another type in this or a parent scope.
The parent type is "subclassed" by applying new attributes and optional metadata.

=back

=cut

has _types        => ( is => 'rw', default => sub { [] } );
has _type_by_name => ( is => 'rw', default => sub { +{} } );

sub type_count {
	scalar @{ shift->_types };
}

sub get_type {
	$_[0]->_types->[$_[1]] // croak "Type idx out of bounds: $_[1]"
}

sub find_type {
	my ($self, $name)= @_;
	my $scope= $self;
	my $t;
	do {
		return $t if $t= $scope->_type_by_name->{$name};
		$scope= $scope->parent;
	} while $scope;
	return undef;
}

sub define_type {
	my ($self, $base, $name, @attrs)= @_;
	Userp::Error::Readonly->throw({ message => 'Cannot alter type table of a finalized scope' })
		if $self->final;
	if (@attrs & 1) {
		@attrs= (ref $attrs[0] eq 'HASH')? %{$attrs[0]} : @{$attrs[0]}
			if @attrs == 1 && ref $attrs[0];
		croak "Expected even number of key/val arguments" if @attrs & 1;
	}
	push @attrs, name => $name, scope_idx => $self->scope_idx, table_idx => $self->type_count;
	my $type;
	# First argument is either a type object, type name in current scope, or a class name in Userp::Type::
	if (ref $base && ref($base)->can('subtype')) {
		$type= $base->subtype(@attrs);
	}
	elsif (($type= $self->find_type($base))) {
		$type= $type->subtype(@attrs);
	}
	elsif ("Userp::Type::$base"->can('new')) {
		$type= "Userp::Type::$base"->new(@attrs);
	}
	else {
		Userp::Error::NotInScope->throw({ message => "No type '$base' in current scope" });
	}
	$type->_register_symbols($self);
	push @{ $self->_types }, $type;
	$self->_type_by_name->{$name}= $type
		if defined $name;
	return $type;
}

sub contains_type {
	my ($self, $type)= @_;
	$type->scope_idx <= $self->scope_idx
		&& $self->scope_stack->[$type->scope_idx]->_types->[$type->table_idx] == $type;
}

=head2 Type Shortcuts

Several types exist in every scope.  These accessors provide slightly more efficient access
to them than L</find_type>.

=over

=item type_Any

The Choice of any defined type.

=item type_Integer

The type of all Integers

=item type_Symbol

The type of all Symbols in scope

=item type_Array

An Array type of one dimension with element type of Any.

=item type_Record

A Record type of undeclared fields.

=back

=cut

sub BUILD {
	my $self= shift;
	# If this is the root scope, initialize the type table with protocol defaults
	if (!$self->parent) {
		my $table= $self->_types;
		push @$table, @Userp::RootTypes::TABLE;
		for (@$table) {
			$_->_register_symbols($self);
			$self->_type_by_name->{$_->name}= $_ if defined $_->name;
		}
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
