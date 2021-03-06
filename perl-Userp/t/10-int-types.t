use Test2::V0;
use Userp::Type::Integer;

my @tests= (
	#    constructor
	#    min  effective_min, max effective_max, bits, effective_bits, align, effective_align  
	[
		{ name => 'Int' },
		undef, undef,     undef, undef,   undef, undef,       undef, 0,
	],
	[
		{ name => 'Positive', parent => 'Int', min => 0 },
		0, 0,             undef, undef,   undef, undef,       undef, 0,
	],
	[
		{ name => 'Negative', parent => 'Int', max => -1 },
		undef, undef,     -1, -1,         undef, undef,       undef, 0,
	],
	[
		{ name => 'int8', parent => 'Int', bits => 8 },
		undef, -128,      undef, 127,     8, 8,               undef, -3,
	],
	[
		{ name => 'bit', parent => 'Int', bits => 1, min => 0 },
		0, 0,             undef, 1,       1, 1,               undef, -3,
	],
	[
		{ name => 'int0_FF', parent => 'Int', min => 0, max => 0xFF },
		0, 0,             0xFF, 0xFF,     undef, 8,           undef, -3,
	],
	[
		{ name => 'int8u', parent => 'Int', bits => 8, min => 0 },
		0, 0,             undef, 0xFF,    8, 8,               undef, -3,
	],
	[
		{ name => 'int8u', parent => 'int8', min => 0 },
		0, 0,             undef, 0xFF,    8, 8,               undef, -3,
	],
	# TODO: test errors in specifications
);

my %types;
for (@tests) {
	my ($ctor, $min, $e_min, $max, $e_max, $bits, $e_bits, $align, $e_align)= @$_;
	my $type= $ctor->{parent}? $types{delete($ctor->{parent})}->subtype(id => 0, %$ctor)
		: Userp::Type::Integer->new(id => 0, %$ctor);
	is( $type, object {
		call name => $ctor->{name};
		call min => $min;
		call effective_min => $e_min;
		call max => $max;
		call effective_max => $e_max;
		call bits => $bits;
		call effective_bits => $e_bits;
		call align => $align;
		call effective_align => $e_align;
	}, $type->name);
	$types{$type->name}= $type;
}

done_testing;
