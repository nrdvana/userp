package Userp::Scope;
use Moo;
use Carp;
with 'Userp::PP::ScopeImpl'
	unless eval {
		require Userp::XS::Scope;
		push @Userp::Scope::ISA, 'Userp::XS::Scope';
	};

=head1 SYNOPSIS

  my $scope= Userp::Scope->new(
	id     => $id_num,      # ID of block that declared scope (0 for root scope)
    parent => $other_scope, # undef if root scope
  );
  $scope->add_symbols(\@symbols);
  $scope->add_types(\@type_definitions);
  $scope->set_block_root_type( $scope->get_type('MyDataFrame') );
  syswrite($fh, $scope->encode_as_metadata_block);

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

=head2 block_root_type

The data type encoded at the root of every block using this scope.  For the highest flexibility,
this can be set to type "Any", in which case the block begins with a type code and can contain
anything you like.  At the opposite extreme, if C<block_type> is "Integer" then each block in
this scope will contain only a single integer.

This can be initialized as either a Type object or ID or identifier name of a type.  After
creation, it will be a reference to a type.

=head2 block_has_id

Each block can be tagged with an ID, with C<0> indicating an anonymous block.  If all your
blocks will be anonymous, you can prevent needing to encode that C<0> each time by setting
this attribute to true.  Note that anonymous blocks cannot contain nested scopes.

=head2 block_implied_scope

=head1 DERIVED ATTRIBUTES

=head2 symbol_max_id

The highest defined symbol ID in the symbol table.

=head2 total_type_count

The highest defined type in the type table.

=cut

has id                  => ( is => 'ro',  required => 1 );
has parent              => ( is => 'ro',  required => 1 );
has block_root_type     => ( is => 'rwp' );
has block_has_id        => ( is => 'rwp' );
has block_implied_scope => ( is => 'rwp' );

# If user passes 'parent', derive some defaults from it.
sub BUILDARGS {
	my $class= shift;
	my $args= $class->next::method(@_);
	if ($args->{parent}) {
		my $p= $args->{parent};
		$args->{id}= $p->block_has_id? $p->next_block_id : undef
			unless exists $args->{id};
		$args->{block_root_type}= $args->{parent}->block_root_type
			unless exists $args->{block_root_type};
		$args->{block_has_id}= $args->{parent}->block_has_id
			unless exists $args->{block_has_id};
		$args->{block_implied_scope}= $args->{parent}->block_implied_scope
			unless exists $args->{block_implied_scope};
	}
	$args;
}

# needs to exist for method wrapping
sub BUILD {
	my $self= shift;
}

=head1 METHODS

=head2 symbol_id

  my $id= $scope->symbol_id("String");
  # undef if "String" is not in scope

=head2 symbol_by_id

  my $string= $scope->symbol_by_id($id);
  # dies if $id is out of range

=head2 type_by_id

  my $type= $scope->type_by_id($id);
  # dies if $id out of range

=head2 type_by_name

  my $type= $scope->type_by_name("Name");
  # undef if no type by that name

=cut

1;
