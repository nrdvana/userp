use Test2::V0;
use Userp::Scope;

subtest ctor => sub {
	my ($parent, $scope);
	is( $parent= Userp::Scope->new, object {
		call parent => undef;
		call final  => undef;
		call scope_stack => array {
			item $parent;
		};
		call scope_idx => 0;
	});
};

done_testing;
