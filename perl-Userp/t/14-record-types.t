use Test2::V0;
use Userp::Type::Integer;
use Userp::Type::Array;
use Userp::Type::Record qw( ALWAYS OFTEN SELDOM );

my %types;
my $int2= Userp::Type::Integer->new(name => 'Int2', bits => 2);
my $int8= Userp::Type::Integer->new(name => 'Int8', bits => 8);
my $str_t= Userp::Type::Array->new(elem_type => $int8, dim => [undef]);

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
		object {
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
		}
    ],
	[
		{
			name => 'SortFields',
			fields => [
				[ f1 => $str_t ],
				[ f2 => $str_t, SELDOM ],
				[ f3 => $int8, OFTEN ],
				'f4',
				{ name => 'f5' },
				[ f6 => $str_t, ALWAYS ],
				[ f7 => $int8, 8 ],
				[ f8 => $int8, 0 ],
			]
		},
		object {
			call fields => array {
				# static fields come first
				item 0 => object {
					call name => 'f7';
					call type => exact_ref $int8;
					call placement => 8;
				};
				item 1 => object {
					call name => 'f8';
					call type => exact_ref $int8;
					call placement => 0;
				};
				# Followed by ALWAYS fields (which is the default)
				item 2 => object {
					call name => 'f1';
					call type => exact_ref $str_t;
					call placement => ALWAYS;
				};
				item 3 => object {
					call name => 'f6';
					call type => exact_ref $str_t;
					call placement => ALWAYS;
				};
				# Followed by OFTEN fields
				item 4 => object {
					call name => 'f3';
					call type => exact_ref $int8;
					call placement => OFTEN;
				};
				item 5 => object {
					call name => 'f4';
					call type => exact_ref $int8;
					call placement => OFTEN;
				};
				item 6 => object {
					call name => 'f5';
					call type => exact_ref $int8;
					call placement => OFTEN;
				};
				# Followed by SELDOM
				item 7 => object {
					call name => 'f2';
					call type => exact_ref $str_t;
					call placement => SELDOM;
				};
			};
			call bitlen => undef;
			call effective_align => 0;
			call effective_static_bits => 16;
		}
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
