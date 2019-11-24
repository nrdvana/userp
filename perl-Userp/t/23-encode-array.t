use Test2::V0;
use Data::Printer;
use Userp::Scope;
use Userp::Encoder;
#use Userp::Decoder;

sub _dump_hex { join ' ', map sprintf("%02X", $_), unpack 'C*', $_[0] }

my $scope= Userp::Scope->new();
isa_ok( $scope, 'Userp::Scope' );
for (
	[ 'Array[Int]', { elem_type => $scope->type_Integer, dim => [undef] }, [
		[ [0], [], "\x00", "\x00" ],
		[ [1], [ 0 ], "\x02\x00", "\x01\x00" ],
		[ [1], [ 1 ], "\x02\x04", "\x01\x02" ],
	]],
	[ 'Array[Int,Int]', { elem_type => $scope->type_Integer, dim => [undef,4] }, [
		[ [0,4], [], "\x00", "\x00" ],
		[ [2,4], [ 1,1,1,1, 2,2,2,2 ], "\x04\x04\x04\x04\x04\x08\x08\x08\x08", "\x02\x02\x02\x02\x02\x04\x04\x04\x04" ],
	]],
	[ 'Array(Int)[]...', { elem_type => $scope->type_Integer }, [
		[ [0], [], "\x02\x00", "\x01\x00" ],
		[ [1], [0], "\x02\x02\x00", "\x01\x01\x00" ],
		[ [1,1], [0], "\x04\x02\x02\x00", "\x02\x01\x01\x00" ],
		[ [2,3], [1,2,3,4,5,6], "\x04\x04\x06\x04\x08\x0C\x10\x14\x18", "\x02\x02\x03\x02\x04\x06\x08\x0A\x0C" ],
	]],
	[ 'Array1', { dim => [undef] }, [
		[ ['Integer',3], [ 1,1,1 ], "\x04\x06\x04\x04\x04", "\x02\x03\x02\x02\x02" ],
	]],
) {
	my ($name, $attrs, $tests)= @$_;
	subtest $name => sub {
		my $t= $scope->add_type($name, 'Array', $attrs);
		isa_ok( $t, 'Userp::Type::Array' );
		for (@$tests) {
			my ($dims, $array, $le_expected, $be_expected)= @$_;
			my $name= '['.join(',', @$array).']';
			
			my $enc_le= Userp::Encoder->new(scope => $scope, current_type => $t);
			$enc_le->begin(@$dims);
			$enc_le->int($_) for @$array;
			$enc_le->end;
			is( $enc_le->buffer, $le_expected, "encode $name little-endian" )
				or diag "encoded as: "._dump_hex($enc_le->buffer);
			
			my $enc_be= Userp::Encoder->new(scope => $scope, current_type => $t, bigendian => 1);
			$enc_be->begin(@$dims);
			$enc_be->int($_) for @$array;
			$enc_be->end;
			is( $enc_be->buffer, $be_expected, "encode $name big-endian" )
				or diag "encoded as: "._dump_hex($enc_be->buffer);
		}
	};
}

done_testing;
