use Test2::V0;
use Data::Printer;
use Userp::Scope;
use Userp::Encoder;
#use Userp::Decoder;

sub _dump_hex { join ' ', map sprintf("%02X", $_), unpack 'C*', $_[0] }

my $scope= Userp::Scope->new();
isa_ok( $scope, 'Userp::Scope' );
for (
	[ 'Integer',  {}, [
		[        0, "\x00",             "\x00" ],
		[        1, "\x04",             "\x02" ],
		[       -1, "\x06",             "\x03" ],
		[        2, "\x08",             "\x04" ],
		[       -2, "\x0A",             "\x05" ],
	]],
	[ 'Positive', { min => 0 }, [
		[        0, "\x00",             "\x00" ],
		[        1, "\x02",             "\x01" ],
		[        2, "\x04",             "\x02" ],
		[      127, "\xFE",             "\x7F" ],
		[      128, "\x01\x02",         "\x80\x80" ],
	]],
	[ 'Int8',     { bits => 8 }, [
		[         0, "\x00", "\x00" ],
		[         1, "\x01", "\x01" ],
		[       127, "\x7F", "\x7F" ],
		[      -128, "\x80", "\x80" ],
	]],
) {
	my ($name, $attrs, $tests)= @$_;
	subtest $name => sub {
		my $t= $scope->define_type('Integer', $name, $attrs);
		isa_ok( $t, 'Userp::Type::Integer' );
		for (@$tests) {
			my $enc_le= Userp::Encoder->new(scope => $scope, current_type => $t);
			my $str= $enc_le->int($_->[0])->buffers->to_string;
			is( $str, $_->[1], "encode $_->[0] little-endian" )
				or diag "encoded as: "._dump_hex($str);
			
			my $enc_be= Userp::Encoder->new(scope => $scope, current_type => $t, bigendian => 1);
			my $str= $enc_be->int($_->[0])->buffers->to_string;
			is( $str, $_->[2], "encode $_->[0] big-endian" )
				or diag "encoded as: "._dump_hex($str);
		}
	};
}

done_testing;
