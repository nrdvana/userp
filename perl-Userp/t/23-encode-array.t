use Test2::V0;
use Data::Printer;
use Userp::Scope;
use Userp::Encoder;
#use Userp::Decoder;

sub _dump_hex { join ' ', map sprintf("%02X", $_), unpack 'C*', $_[0] }

my $parent= Userp::Scope->new;
my $scope= Userp::Scope->new(parent => $parent);
isa_ok( $scope, 'Userp::Scope' );
is( $scope->type_count, 0, 'starts empty' );
my $int_tc_le= pack 'C', (($scope->type_Int->table_idx << 1) + 1) << 2;
my $int_tc_be= pack 'C', (($scope->type_Int->table_idx << 1) + 1) << 1;
for (
	[ 'Array[Int]', { elem_type => $scope->type_Int, dim => [undef] }, [
		[ [0], [], "\x00", "\x00" ],
		[ [1], [ 0 ], "\x02\x00", "\x01\x00" ],
		[ [1], [ 1 ], "\x02\x04", "\x01\x02" ],
	]],
	[ 'Array[Int,Int]', { elem_type => $scope->type_Int, dim => [undef,4] }, [
		[ [0,4], [], "\x00", "\x00" ],
		[ [2,4], [ 1,1,1,1, 2,2,2,2 ], "\x04\x04\x04\x04\x04\x08\x08\x08\x08", "\x02\x02\x02\x02\x02\x04\x04\x04\x04" ],
	]],
	[ 'Array(Int)[]...', { elem_type => $scope->type_Int }, [
		[ [0], [], "\x02\x00", "\x01\x00" ],
		[ [1], [0], "\x02\x02\x00", "\x01\x01\x00" ],
		[ [1,1], [0], "\x04\x02\x02\x00", "\x02\x01\x01\x00" ],
		[ [2,3], [1,2,3,4,5,6], "\x04\x04\x06\x04\x08\x0C\x10\x14\x18", "\x02\x02\x03\x02\x04\x06\x08\x0A\x0C" ],
	]],
	[ 'Array1', { dim => [undef] }, [
		[ [ $scope->type_Int ,3], [ 1,1,1 ], "$int_tc_le\x06\x04\x04\x04", "$int_tc_be\x03\x02\x02\x02" ],
	]],
) {
	my ($name, $attrs, $tests)= @$_;
	subtest $name => sub {
		my $t= $scope->define_type(Array => $name, $attrs);
		isa_ok( $t, 'Userp::Type::Array' );
		for (@$tests) {
			my ($begin, $array, $le_expected, $be_expected)= @$_;
			my $name= '['.join(',', @$array).']';
			
			my $enc_le= Userp::Encoder->new(scope => $scope, current_type => $t);
			$enc_le->begin_array(@$begin);
			$enc_le->int($_) for @$array;
			$enc_le->end_array;
			is( $enc_le->buffers->to_string, $le_expected, "encode $name little-endian" )
				or diag "encoded as: "._dump_hex($enc_le->buffers->to_string);
			
			my $enc_be= Userp::Encoder->new(scope => $scope, current_type => $t, bigendian => 1);
			$enc_be->begin_array(@$begin);
			$enc_be->int($_) for @$array;
			$enc_be->end_array;
			is( $enc_be->buffers->to_string, $be_expected, "encode $name big-endian" )
				or diag "encoded as: "._dump_hex($enc_be->buffers->to_string);
		}
	};
}

done_testing;
