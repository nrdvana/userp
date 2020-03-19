use Test2::V0;
use Data::Printer;
use Userp::Scope;
use Userp::Encoder;
use Userp::Type::Record qw( ALWAYS OFTEN SELDOM );

sub _dump_hex { join ' ', map sprintf("%02X", $_), unpack 'C*', $_[0] }

my $parent= Userp::Scope->new;
my $scope= Userp::Scope->new(parent => $parent);
isa_ok( $scope, 'Userp::Scope' );
is( $scope->type_count, 0, 'starts empty' );
$scope->add_symbols(qw( a b c ));
my $IntU= $scope->type_IntU;
my $IntU8= $scope->define_type($IntU, 'IntU8', bits => 8);
my $IntU3= $scope->define_type($IntU, 'IntU3', bits => 3);

my $sym_a_le=  pack 'C', (($scope->find_symbol('a') << 1) + 1) << 2;
my $sym_a_be=  pack 'C', (($scope->find_symbol('a') << 1) + 1) << 1;
my $sym_b_le=  pack 'C', (($scope->find_symbol('b') << 1) + 1) << 2;
my $sym_b_be=  pack 'C', (($scope->find_symbol('b') << 1) + 1) << 1;
my $sym_c_le=  pack 'C', (($scope->find_symbol('c') << 1) + 1) << 2;
my $sym_c_be=  pack 'C', (($scope->find_symbol('c') << 1) + 1) << 1;
my $int_le= pack 'C', (($scope->type_Int->table_idx << 1) + 1) << 2;
my $int_be= pack 'C', (($scope->type_Int->table_idx << 1) + 1) << 1;
for (
	[ 'Record of EXTRA', { extra_field_type => $IntU }, [
		[ [], [], {}, "\x00", "\x00" ],
		[ ['a'], ['a'], { a => 0 }, "\x02$sym_a_le\x00", "\x01$sym_a_be\x00" ],
		[ [qw( b c a )], [qw( b c a )], { a => 0, b => 1, c => 2 },
			"\x06$sym_b_le$sym_c_le$sym_a_le\x02\x04\x00",
			"\x03$sym_b_be$sym_c_be$sym_a_be\x01\x02\x00"
		],
	]],
	[ 'Record of ALWAYS', { fields => [[ a => $IntU ], [ b => $IntU ]] }, [
		[ ['a','b'], ['a','b'], { a => 2, b => 1 }, "\x04\x02", "\x02\x01" ],
		[ ['b','a'], ['b','a'], { a => 2, b => 1 }, "\x04\x02", "\x02\x01" ],
		[ [],        ['a','b'], { a => 2, b => 1 }, "\x04\x02", "\x02\x01" ],
	]],
	[ 'Record of OFTEN', { fields => [[ a => $IntU, OFTEN ], [ b => $IntU8 ], [ c => $IntU3 ]] }, [
		[ [],        [],        { a => 1, b => 0xFF, c => 7 }, "\0", "\0" ],
		[ ['a'],     ['a'],     { a => 1, b => 0xFF, c => 7 }, "\x01\x02", "\x20\x01" ],
		[ ['b'],     ['b'],     { a => 1, b => 0xFF, c => 7 }, "\xFA\x07", "\x5F\xE0" ],
		[ ['c'],     ['c'],     { a => 1, b => 0xFF, c => 7 }, "\x3C",     "\x9C" ],
	]],
	[ 'Record of >64 OFTEN fields',
		{ fields => [ map [ "f$_" => $IntU8, OFTEN ], 0..65 ]},
		[
			[ ['f42'],   ['f42'],   { f42 => 1 }, "\0\0\0\0\0\x04\0\0\x04\0", "\0\0\x01\0\0\0\0\0\0\x40" ]
		]
	],
) {
	my ($name, $attrs, $tests)= @$_;
	subtest $name => sub {
		my $t= $scope->define_type(Record => $name, $attrs);
		isa_ok( $t, 'Userp::Type::Record' );
		for (@$tests) {
			my ($begin, $fields, $record, $le_expected, $be_expected)= @$_;
			my $name= '{'.join(',', map "$_=$record->{$_}", @$fields).'}';
			
			my $enc_le= Userp::Encoder->new(scope => $scope, current_type => $t);
			$enc_le->begin_record(@$begin);
			$enc_le->int($record->{$_}) for @$fields;
			$enc_le->end_record;
			is( $enc_le->buffers->to_string, $le_expected, "encode $name little-endian" )
				or diag "encoded as: "._dump_hex($enc_le->buffers->to_string);
			
			my $enc_be= Userp::Encoder->new(scope => $scope, current_type => $t, bigendian => 1);
			$enc_be->begin_record(@$begin);
			$enc_be->int($record->{$_}) for @$fields;
			$enc_be->end_record;
			is( $enc_be->buffers->to_string, $be_expected, "encode $name big-endian" )
				or diag "encoded as: "._dump_hex($enc_be->buffers->to_string);
		}
	};
}

done_testing;
