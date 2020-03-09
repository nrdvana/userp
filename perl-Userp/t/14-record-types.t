use Test2::V0;
use Userp::Type::Integer;
use Userp::Type::Record;

my %types;
my $int2= Userp::Type::Integer->new(name => 'Int2', bits => 2);
my $int8= Userp::Type::Integer->new(name => 'Int8', bits => 8);

my @tests= (
	[
		{
			name => 'TwoBitFields',
			fields => [
				[ f1 => $int2 ],
				[ f2 => $int2 ],
				[ f3 => $int2 ],
				[ f4 => $int8 ],
			],
		},
		(object {
			call fields => array {
				item 0 => object {
					call name => 'f1';
					call idx => 0;
				};
				etc;
			};
		})
    ]
);
for (@tests) {
	my ($ctor, $check)= @$_;
	my $type= $ctor->{parent}? $types{delete($ctor->{parent})}->subtype($ctor)
		: Userp::Type::Record->new($ctor);
	is( $type, $check, $type->name );
	$types{$type->name}= $type;
}

done_testing;
