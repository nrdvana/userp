use Test2::V0;
use Data::Printer;
use Userp::PP::Decoder;

my @tests= (
	# bits, signed, bigendian, value, encoding
	[   1, 0,          0,                     "\0" ],
	[   2, 0,          0,                     "\0" ],
	[   7, 0,          0,                     "\0" ],
	[   8, 0,          0,                     "\0" ],
	[   9, 0,          0,                   "\0\0" ],
	[   9, 0,       0xFF,                 "\xFF\0" ],
	[   9, 1,       0xFF,                 "\0\xFF" ],
	[   9, 0,      0x100,               "\x00\x01" ],
	[  32, 0,      0x100,             "\0\x01\0\0" ],
	[  64, 0,      0x100,     "\0\x01\0\0\0\0\0\0" ],
	[  65, 0,      0x100,   "\0\x01\0\0\0\0\0\0\0" ],
	# var-len ints
	[   0, 0,          0,                     "\0" ],
	[   0, 0,          1,                   "\x02" ],
	[   0, 1,          1,                   "\x01" ],
	[   0, 0,       0x7F,                   "\xFE" ],
	[   0, 1,       0x7F,                   "\x7F" ],
	[   0, 0,   0x121212,         "\x93\x90\x90\0" ],
	[   0, 1,   0x121212,       "\xC0\x12\x12\x12" ],
);
for (@tests) {
	my ($bits, $be, $value, $encoding)= @$_;
	my $decoder= Userp::PP::Decoder->new(bigendian => $be, scope => undef, buffer => \$encoding);
	my $x;
	if ($bits > 0) {
		is( ''.$decoder->decode_intN($bits), $value, "bits=$bits be=$be value=$value" );
	} else {
		is( ''.$decoder->decode_intX, $value, "var-int be=$be value=$value" );
	}
}

done_testing;
