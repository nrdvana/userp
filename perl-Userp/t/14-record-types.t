use Test2::V0;
use Userp::Type::Integer;
use Userp::Type::Record;

my %types;
my $int2= Userp::Type::Integer->new(id => 0, name => 'Int2', bits => 2);
my $int8= Userp::Type::Integer->new(id => 0, name => 'Int8', bits => 8);

my @tests= (
	[
		{
			name => 'TwoBitFields',
			sequential_fields => [
				[ f1 => $int2 ],
				[ f2 => $int2 ],
				[ f3 => $int2 ],
				[ f4 => $int8 ],
			],
			adhoc_fields => 0,
		},
		object {
			call [ get_field => 'f1' ] => object {
				call name => 'f1';
				call idx => 0;
			};
		}
    ]
);
for (@tests) {
	my ($ctor, $check)= @$_;
	my $type= $ctor->{parent}? $types{delete($ctor->{parent})}->subtype(id => 0, %$ctor)
		: Userp::Type::Record->new(id => 0, %$ctor);
	is( $type, $check, $type->name );
	$types{$type->name}= $type;
}

done_testing;
