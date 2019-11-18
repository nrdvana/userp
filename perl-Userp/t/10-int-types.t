use Test2::V0;
use Data::Printer;
use Userp::Scope;
use Userp::Encoder;
#use Userp::Decoder;

sub _dump_hex { join ' ', map sprintf("%02X", $_), unpack 'C*', $_[0] }

subtest Int8 => sub {
	my $scope= Userp::Scope->new();
	isa_ok( $scope, 'Userp::Scope' );
	my $Int8= $scope->add_type('Int8', 'Integer', { twosc => 8 });
	isa_ok( $Int8, 'Userp::Type::Integer' );
	my $enc= Userp::Encoder->new(scope => $scope, current_type => $Int8);
	isa_ok( $enc, 'Userp::Encoder' );
	$enc->int(5);
	is( $enc->buffer, "\x05" ) or diag _dump_hex($enc->buffer);
};

done_testing;
