use Test2::V0;
use Data::Printer;
use Userp::Bits;
use Userp::Buffer;

sub main::_dump_hex { join ' ', map sprintf("%02X", $_), unpack 'C*', $_[0] }
sub bigint { Math::BigInt->new(shift) }
sub stringify_testvals { join ' ', map { ref $_ eq 'ARRAY'? '['.join(',', @$_).']' : $_ } @{$_[0]} }

SKIP: {
	skip "Bits64 not supported" unless eval "pack('Q<',1)";
	subtest Bits64 => sub {
		require Userp::PP::Bits64;
		test_bits_api('Userp::PP::Bits64');
	};
}

todo "Bits32 not written" => sub {
	subtest Bits32 => sub {
		require Userp::PP::Bits32;
		test_bits_api('Userp::PP::Bits32');
	};
};

sub test_bits_api {
	my $class= shift;
	for my $fn (qw(
		sign_extend
		bitlen
		unsigned_max
		roundup_bits_to_alignment
		pack_bits
		ints
	)) {
		subtest $fn => sub { __PACKAGE__->can("test_$fn")->($class) };
	}
}

sub test_sign_extend {
	my $sign_extend= shift->can('sign_extend') || die;
	my @tests= (
		[ 0, 5, 0 ],
		[ 1, 5, 1 ],
		[ 1, 1, -1 ],
		[ 0x4, 3, -4 ],
		[ 0xF0, 8, -16 ],
		[ 0x7FFF_FFFF, 31, -1 ],
		[ 0x7FFF_FFFF, 32, 0x7FFF_FFFF ],
		[ 0xFFFF_FFFF, 32, -1 ],
		[ Math::BigInt->new(1)->blsft(72)->bdec, 72, -1 ],
		[ Math::BigInt->new(1)->blsft(72)->bdec, 73, Math::BigInt->new(1)->blsft(72)->bdec ],
	);
	for (@tests) {
		my ($val, $bits, $expected)= @$_;
		is( $sign_extend->($val, $bits), $expected, "sign_extend($val, $bits)" );
	}
	done_testing;
}

sub test_bitlen {
	my $bitlen_fn= shift->can('bitlen') || die;
	my @tests= (
		[ 0, 0 ],
		[ 1, 1 ],
		[ 2, 2 ],
		[ 3, 2 ],
		[ 4, 3 ],
		[ 5, 3 ],
		[ 6, 3 ],
		[ 7, 3 ],
		[ 8, 4 ],
		[ 9, 4 ],
		[ 15, 4 ],
		[ 16, 5 ],
		[ 31, 5 ],
		[ 32, 6 ],
		[ 63, 6 ],
		[ 64, 7 ],
		[ 127, 7 ],
		[ 128, 8 ],
		[ 255, 8 ],
		[ 256, 9 ],
		[ 511, 9 ],
		[ 512, 10 ],
		[ 1023, 10 ],
		[ 1024, 11 ],
		[ 2047, 11 ],
		[ 2048, 12 ],
		[ 4095, 12 ],
		[ 4096, 13 ],
	);
	for (@tests) {
		my ($val, $bitlen)= @$_;
		my $x= $bitlen_fn->($val);
		is( $x, $bitlen, $val );
	}
}

sub test_unsigned_max {
	my $class= shift;
	my ($unsigned_max)= map $class->can($_), qw( unsigned_max );
	my @tests= (
		[ 0, 0 ],
		[ 1, 1 ],
		[ 2, 3 ],
		[ 3, 7 ],
		[ 4, 0x0F ],
		[ 5, 0x1F ],
		[ 6, 0x3F ],
		[ 7, 0x7F ],
		[ 8, 0xFF ],
		[ 9, 0x1FF ],
		[ 15, 0x7FFF ],
		[ 16, 0xFFFF ],
		[ 31, 0x7FFFFFFF ],
		[ 32, 0xFFFFFFFF ],
		[ 63, '9223372036854775807' ],
		[ 64, '18446744073709551615' ],
		[ 65, '36893488147419103231' ],
	);
	for (@tests) {
		my ($bits, $max)= @$_;
		my $x= $unsigned_max->($bits);
		is( "$x", $max, "2**$bits" );
	}
}

sub test_roundup_bits_to_alignment {
	my $class= shift;
	my ($roundup)= map $class->can($_), qw( roundup_bits_to_alignment );
	my @tests= (
		[ 1, -3 => 1 ],
		[ 1, -2 => 2 ],
		[ 0, -1 => 0 ],
		[ 1, -1 => 4 ],
		[ 0, 0 => 0 ],
		[ 1, 0 => 8 ],
		[ 1, 1 => 16 ],
		[ 1, 2 => 32 ],
	);
	for (@tests) {
		my ($bits, $align, $expect)= @$_;
		my $x= $roundup->($bits, $align);
		is( $x, $expect, "round $bits bits to 2**$align bytes" );
	}
}

sub test_pack_bits {
	my $class= shift;
	my ($pack_bits_le, $pack_bits_be)= map $class->can($_), qw( pack_bits_le pack_bits_be );
	my @tests= (
		# value, bits, encoding_le encoding_be
		[ 0,  1, "\x00", "\x00" ],
		[ 1,  1, "\x01", "\x80" ],
		[ 1,  2, "\x01", "\x40" ],
		[ 1,  3, "\x01", "\x20" ],
		[ 1,  4, "\x01", "\x10" ],
		[ 1,  5, "\x01", "\x08" ],
		[ 1,  6, "\x01", "\x04" ],
		[ 1,  7, "\x01", "\x02" ],
		[ 1,  8, "\x01", "\x01" ],
		[ 1,  9, "\x01\x00", "\x00\x80" ],
		[ 1, 15, "\x01\x00", "\x00\x02" ],
		[ 1, 16, "\x01\x00", "\x00\x01" ],
		[ 1, 17, "\x01\x00\x00", "\x00\x00\x80" ],
		[ 1, 23, "\x01\x00\x00", "\x00\x00\x02" ],
		[ 1, 24, "\x01\x00\x00", "\x00\x00\x01" ],
		[ 1, 25, "\x01\x00\x00\x00", "\x00\x00\x00\x80" ],
		[ 1, 31, "\x01\x00\x00\x00", "\x00\x00\x00\x02" ],
		[ 1, 32, "\x01\x00\x00\x00", "\x00\x00\x00\x01" ],
		[ 1, 33, "\x01\x00\x00\x00\x00", "\x00\x00\x00\x00\x80" ],
		[ 1, 63, "\x01\x00\x00\x00\x00\x00\x00\x00", "\x00\x00\x00\x00\x00\x00\x00\x02" ],
		[ 1, 64, "\x01\x00\x00\x00\x00\x00\x00\x00", "\x00\x00\x00\x00\x00\x00\x00\x01" ],
		[ 1, 65, "\x01\x00\x00\x00\x00\x00\x00\x00\x00", "\x00\x00\x00\x00\x00\x00\x00\x00\x80" ],
	);
	for (@tests) {
		my ($value, $bits, $le, $be)= @$_;
		is( $pack_bits_le->($bits, $value), $le, "pack_bits_le($bits,$value)" );
		is( $pack_bits_be->($bits, $value), $be, "pack_bits_be($bits,$value)" );
	}
}

sub test_ints {
	my $class= shift;
	my ($enc_int_le, $enc_int_be, $dec_int_le, $dec_int_be)=
		map $class->can($_), qw( encode_int_le encode_int_be decode_int_le decode_int_be );
	my @tests= (
		# value_and_bits_list, encoding_le, encoding_be
		[ [ [1,1], [1,1] ], "\x03", "\xC0" ],
		[ [ [4,3], [7,3] ], "\x3C", "\x9C" ],
		[ [ [1,3], [1,9] ], "\x09\x00", "\x20\x10" ],
		[ [ [7,3], [7,3], [3,2], [1,1] ], "\xFF\x01", "\xFF\x80" ],
		[ [ [7,3], [7,3], [7,3] ], "\xFF\x01", "\xFF\x80" ],
		[ [ [1,8], [0x80,8], [0x7F,8], [0xFF,8] ], "\x01\x80\x7F\xFF", "\x01\x80\x7F\xFF" ],
		[ [ [1,16], [0x8000,16], [0x7FFF,16], [0xFFFF,16] ], "\x01\x00\x00\x80\xFF\x7F\xFF\xFF", "\x00\x01\x80\x00\x7F\xFF\xFF\xFF" ],
		[ [ [1,32], [0x80000000,32] ], "\x01\x00\x00\x00\x00\x00\x00\x80", "\x00\x00\x00\x01\x80\x00\x00\x00" ],
		[ [ [0x7FFFFFFF,32], [0xFFFFFFFF,32] ], "\xFF\xFF\xFF\x7F\xFF\xFF\xFF\xFF", "\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ],
		[ [ [bigint(1),64] ], "\x01\x00\x00\x00\x00\x00\x00\x00", "\x00\x00\x00\x00\x00\x00\x00\x01" ],
		[ [ [bigint(1)<<63,64] ], "\x00\x00\x00\x00\x00\x00\x00\x80", "\x80\x00\x00\x00\x00\x00\x00\x00" ],
		[ [ [(bigint(1)<<63)-1,64] ], "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x7F", "\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ],
		[ [ [(bigint(1)<<64)-1,64] ], "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ],
		# variable-length
		[ [ [1] ], "\x02", "\x01" ],
		[ [ [0x7F] ], "\xFE", "\x7F" ],
		[ [ [0x80] ], "\x01\x02", "\x80\x80" ],
		[ [ [0xFF] ], "\xFD\x03", "\x80\xFF" ],
		[ [ [0x0FFF] ], "\xFD\x3F", "\x8F\xFF" ],
		[ [ [0x3FFF] ], "\xFD\xFF", "\xBF\xFF" ],
		[ [ [0x4000] ], "\x03\x00\x02\x00", "\xC0\x00\x40\x00" ],
		[ [ [0x1F00_0000] ], "\x03\x00\x00\xF8", "\xDF\x00\x00\x00" ],
		[ [ [0x1FFF_FFFF] ], "\xFB\xFF\xFF\xFF", "\xDF\xFF\xFF\xFF" ],
		[ [ [0x2000_0000] ], "\x07\x00\x00\x00\x02\x00\x00\x00", "\xE0\x00\x00\x00\x20\x00\x00\x00" ],
		[ [ [(bigint(1)<<60)-1] ], "\xF7\xFF\xFF\xFF\xFF\xFF\xFF\xFF", "\xEF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ],
		[ [ [(bigint(1)<<60)] ],   "\x0F\x00\x00\x00\x00\x00\x00\x00\x00\x10", "\xF0\x00\x10\x00\x00\x00\x00\x00\x00\x00" ],
		[ [ [(bigint(1)<<64)-1] ], "\x0F\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", "\xF0\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ],
	);
	for (@tests) {
		my ($list, $le, $be)= @$_;
		my $test_str= stringify_testvals($list);
		subtest $test_str => sub {
			my ($buf_le, $buf_be)= (Userp::Buffer->new_le, Userp::Buffer->new_be);
			for (@$list) {
				$enc_int_le->($buf_le, @$_);
				$enc_int_be->($buf_be, @$_);
			}
			is( ${$buf_le->[0]}, $le, "encode_int_le $test_str" )
				or diag _dump_hex(${$buf_le->[0]});
			is( ${$buf_be->[0]}, $be, "encode_int_be $test_str" )
				or diag _dump_hex(${$buf_be->[0]});
			($buf_le, $buf_be)= (Userp::Buffer->new_le(\$le), Userp::Buffer->new_be(\$be));
			for (@$list) {
				is( $dec_int_le->($buf_le, $_->[1]), ''.$_->[0], "decode_int_le ".stringify_testvals([$_]) )
					or diag _dump_hex(${$buf_le->bufref});
				is( $dec_int_be->($buf_be, $_->[1]), ''.$_->[0], "decode_int_be ".stringify_testvals([$_]) )
					or diag _dump_hex(${$buf_be->bufref});
			}
		};
	}
}

done_testing;
