use Test2::V0;
use Data::Printer;
use Userp::Scope;
use Userp::Encoder;

sub _dump_hex { (my $x= unpack('H*', $_[0])) =~ s/(..)/ $1/g; $x }

my $root= Userp::Scope->new();
my $scope= Userp::Scope->new(parent => $root);
$scope->add_symbols(qw( Test test Test1 ));
isa_ok( $scope, 'Userp::Scope' );
for (
	[ "Test",    "\x02",        "\x01" ],
	[ "test",    "\x06",        "\x03" ],
	[ "Test1",   "\x0A",        "\x05" ],
) {
	my ($symbol, $encoding_le, $encoding_be)= @$_;
	my $enc_le= Userp::Encoder->new(scope => $scope, current_type => $scope->type_Symbol);
	is( $enc_le->sym($symbol)->buffers->to_string, $encoding_le, "encode $symbol little-endian" )
		or diag "encoded as: "._dump_hex($enc_le->buffers->to_string);
	my $enc_be= Userp::Encoder->new(scope => $scope, current_type => $scope->type_Symbol, bigendian => 1);
	is( $enc_be->sym($symbol)->buffers->to_string, $encoding_be, "encode $symbol big-endian" )
		or diag "encoded as: "._dump_hex($enc_be->buffers->to_string);
}

done_testing;
