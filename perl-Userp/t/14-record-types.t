use Test2::V0;
use Userp::Type::Integer;
use Userp::Type::Record qw( ALWAYS OFTEN SELDOM );

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
					call placement => ALWAYS;
					call idx => 0;
				};
				etc;
			};
			call align => undef;
			call effective_align => -3;
			call effective_static_bits => 0;
			call bitlen => 14;
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
