use Test2::V0;
use Userp::PP::Decoder;

my @tests= (
	# bits, signed, bigendian, value, encoding
	[   1, 0, 0,          0,             "\0" ],
	[   2, 0, 0,          0,             "\0" ],
	[   7, 0, 0,          0,             "\0" ],
	[   8, 0, 0,          0,             "\0" ],
	[   9, 0, 0,          0,           "\0\0" ],
	[   9, 0, 0,       0xFF,         "\xFF\0" ],
	[   9, 0, 1,       0xFF,         "\0\xFF" ],
	[   9, 0, 0,      0x100,       "\x00\x01" ],
);
for (@tests) {
	my ($bits, $signed, $be, $value, $encoding)= @$_;
	my $decoder= Userp::PP::Decoder->new(bigendian => $be, scope => undef, buffer => \$encoding);
	if ($bits > 0) {
		is( $value, $decoder->decode_intN($bits, $signed), "bits=$bits signed=$signed be=$be value=$value" );
	} else {
		is( $value, $decoder->decode_intX($signed) );
	}
}

done_testing;
