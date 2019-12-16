use Test2::V0;
use Data::Printer;
use Userp::Scope;
use Userp::Encoder;
#use Userp::Decoder;

sub _dump_hex { (my $x= unpack('H*', $_[0])) =~ s/(..)/ $1/g; $x }

my $scope= Userp::Scope->new();
$scope->add_symbol($_) for qw(
	Test
);
isa_ok( $scope, 'Userp::Scope' );
for (
	[ "Test",    "\x04",        "\x02" ],
	[ "Test1",   "\x06\x021",   "\x03\x011" ],
	[ "Testing", "\x06\x06ing", "\x03\x03ing" ],
	[ "Foo",     "\x02\x06Foo", "\x01\x03Foo" ],
) {
	my ($symbol, $encoding_le, $encoding_be)= @$_;
	my $enc_le= Userp::Encoder->new(scope => $scope, current_type => $scope->type_Symbol);
	is( $enc_le->str($symbol)->buffer, $encoding_le, "encode $symbol little-endian" )
		or diag "encoded as: "._dump_hex($enc_le->buffer);
	my $enc_be= Userp::Encoder->new(scope => $scope, current_type => $scope->type_Symbol, bigendian => 1);
	is( $enc_be->str($symbol)->buffer, $encoding_be, "encode $symbol big-endian" )
		or diag "encoded as: "._dump_hex($enc_be->buffer);
}

done_testing;
