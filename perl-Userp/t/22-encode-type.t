use Test2::V0;
use Data::Printer;
use Userp::Scope;
use Userp::Encoder;

sub _dump_hex { (my $x= unpack('H*', $_[0])) =~ s/(..)/ $1/g; $x }

my $root= Userp::Scope->new();
my $scope= Userp::Scope->new(parent => $root);
my $test_t= $scope->define_type(Integer => 'Test', min => 0);
for (
	[ $test_t,   "\x02",        "\x01" ],
	[ "Test",    "\x02",        "\x01" ],
) {
	my ($type, $encoding_le, $encoding_be)= @$_;
	my $enc_le= Userp::Encoder->new(scope => $scope, current_type => $scope->type_Type);
	is( $enc_le->typeref($type)->buffers->to_string, $encoding_le, "encode $type little-endian" )
		or diag "encoded as: "._dump_hex($enc_le->buffers->to_string);
	my $enc_be= Userp::Encoder->new(scope => $scope, current_type => $scope->type_Type, bigendian => 1);
	is( $enc_be->typeref($type)->buffers->to_string, $encoding_be, "encode $type big-endian" )
		or diag "encoded as: "._dump_hex($enc_be->buffers->to_string);
}

done_testing;
