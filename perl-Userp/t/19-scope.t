use Test2::V0;
use Userp::Scope;
sub explain { require Data::Dumper; Data::Dumper::Dumper([@_]) }

subtest ctor => sub {
	my ($parent, $scope);
	is( $parent= Userp::Scope->new, object {
		call parent => undef;
		call final  => undef;
		call scope_stack => array {
			item 0 => exact_ref $parent;
		};
		call scope_idx => 0;
	}, 'parent scope' );
	is( $scope= Userp::Scope->new(parent => $parent), object {
		call parent => exact_ref $parent;
		call final => undef;
		call scope_stack => array {
			item 0 => exact_ref $parent;
			item 1 => exact_ref $scope;
		};
		call scope_idx => 1;
	}, 'child scope' );
};

subtest declare_symbols => sub {
	my $root= Userp::Scope->new;
	my $parent= Userp::Scope->new(parent => $root);
	my $scope= Userp::Scope->new(parent => $parent);

	is( $parent->add_symbols('a','b','c','d'), 4, 'add a, b, c, d to parent' );
	is( $parent->symbol_count, 4, 'parent symbol count' );
	is( [$parent->find_symbol('a')], [1,number 0], 'a is [1,0]' );
	is( [$parent->find_symbol('d')], [1,3], 'd is [1,3]' );
	is( [$parent->find_symbol('z')], [], 'z not declared' );
	
	is( $scope->add_symbols('a','b','c','d','e','f','g'), 3, 'added e, f, g to child' )
		or diag explain $scope->_symbols;
	is( $scope->symbol_count, 3, 'scope symbol count' );
	is( [$scope->find_symbol('a')], [1,number 0], 'a is [1,0] in child' );
	is( [$scope->find_symbol('e')], [2,number 0], 'e is [2,0] in child' );
};

subtest declare_types => sub {
	my $root= Userp::Scope->new;
	my $parent= Userp::Scope->new(parent => $root);
	my $scope= Userp::Scope->new(parent => $parent);
	
	my $foo= $parent->define_type(Any => 'Foo');
	is( $foo, object {
		call isa_Any => T;
		call name => 'Foo';
		call scope_idx => 1;
		call table_idx => 0;
	}, 'define Foo' );
	is( $parent->find_type('Foo'), exact_ref $foo, 'can find foo in parent' );
	is( $scope->find_type('Foo'), exact_ref $foo, 'can find foo in child' );
};

done_testing;
