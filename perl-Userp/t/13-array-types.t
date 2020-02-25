use Test2::V0;
use Userp::Type::Integer;
use Userp::Type::Array;

my %types;
my $int2= Userp::Type::Integer->new(name => 'Int2', bits => 2);
my $int8= Userp::Type::Integer->new(name => 'Int8', bits => 8);
my $int32= Userp::Type::Integer->new(name => 'Int32', bits => 32);

my @tests= (
	[
		{
			name => 'Array_of_Byte',
			elem_type => $int8,
		},
		object {
			call dim => undef;
			call elem_type => exact_ref $int8;
			call align => undef;
			call effective_align => 0;
			call bitlen => undef;
		}
    ],
	[
		{
			name => 'Array5_of_Byte',
			elem_type => $int8,
			dim => [5],
		},
		object {
			call dim => [5];
			call elem_type => exact_ref $int8;
			call align => undef;
			call effective_align => 0;
			call bitlen => 40;
		}
	],
	[
		{
			name => 'Array5x?_of_Byte',
			elem_type => $int8,
			dim => [5, undef],
		},
		object {
			call dim => [5, undef];
			call elem_type => exact_ref $int8;
			call align => undef;
			call effective_align => 0;
			call bitlen => undef;
		}
	],
	[
		{
			name => 'Array2x2_of_Int2',
			elem_type => $int2,
			dim => [ 2, 2 ],
		},
		object {
			call dim => [2,2];
			call elem_type => exact_ref $int2;
			call align => undef;
			call effective_align => -2;
			call bitlen => 8;
		}
	],
	[
		{
			name => 'Array8_of_Int32',
			elem_type => $int32,
			dim => [8],
		},
		object {
			call dim => [8];
			call elem_type => exact_ref $int32;
			call align => undef;
			call effective_align => 2;
			call bitlen => 256;
		}
	],
);
for (@tests) {
	my ($ctor, $check)= @$_;
	my $type= $ctor->{parent}? $types{delete($ctor->{parent})}->subtype(%$ctor)
		: Userp::Type::Array->new(%$ctor);
	is( $type, $check, $type->name );
	$types{$type->name}= $type;
}

done_testing;
