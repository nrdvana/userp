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
	subtest sign_extend => sub { test_sign_extend($class) };
	subtest bitlen      => sub { test_bitlen($class) };
	subtest pack_bits   => sub { test_pack_bits($class) };
	subtest bits        => sub { test_bits($class) };
	subtest vqty        => sub { test_vqty($class) };
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
	done_testing;
}

sub test_bits {
	my $class= shift;
	my ($enc_bits_le, $enc_bits_be, $dec_bits_le, $dec_bits_be)=
		map $class->can($_), qw( buffer_encode_bits_le buffer_encode_bits_be buffer_decode_bits_le buffer_decode_bits_be );
	my @tests= (
		# value_and_bits_list, encoding_le, encoding_be
		[ [ [1,1], [1,1] ], "\x03", "\xC0" ],
		[ [ [1,3], [1,9] ], "\x09\x00", "\x20\x10" ],
		[ [ [7,3], [7,3], [3,2], [1,1] ], "\xFF\x01", "\xFF\x80" ],
		[ [ [7,3], [7,3], [7,3] ], "\xFF\x01", "\xFF\x80" ],
		[ [ [1,8], [0x80, 8], [0x7F, 8], [0xFF,8] ], "\x01\x80\x7F\xFF", "\x01\x80\x7F\xFF" ],
		[ [ [1,16], [0x8000,16], [0x7FFF,16], [0xFFFF,16] ], "\x01\x00\x00\x80\xFF\x7F\xFF\xFF", "\x00\x01\x80\x00\x7F\xFF\xFF\xFF" ],
		[ [ [1,32], [0x80000000,32] ], "\x01\x00\x00\x00\x00\x00\x00\x80", "\x00\x00\x00\x01\x80\x00\x00\x00" ],
		[ [ [0x7FFFFFFF,32], [0xFFFFFFFF,32] ], "\xFF\xFF\xFF\x7F\xFF\xFF\xFF\xFF", "\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ],
		[ [ [bigint(1),64] ], "\x01\x00\x00\x00\x00\x00\x00\x00", "\x00\x00\x00\x00\x00\x00\x00\x01" ],
		[ [ [bigint(1)<<63,64] ], "\x00\x00\x00\x00\x00\x00\x00\x80", "\x80\x00\x00\x00\x00\x00\x00\x00" ],
		[ [ [(bigint(1)<<63)-1,64] ], "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x7F", "\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ],
		[ [ [(bigint(1)<<64)-1,64] ], "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ],
	);
	for (@tests) {
		my ($list, $le, $be)= @$_;
		my $test_str= stringify_testvals($list);
		my ($buf_le, $buf_be)= (Userp::Buffer->new, Userp::Buffer->new);
		for (@$list) {
			$enc_bits_le->($buf_le, $_->[1], $_->[0]);
			$enc_bits_be->($buf_be, $_->[1], $_->[0]);
		}
		is( ${$buf_le->[0]}, $le, "encode_bits_le $test_str" );
		is( ${$buf_be->[0]}, $be, "encode_bits_be $test_str" );
	}
	for (@tests) {
		my ($list, $le, $be)= @$_;
		my $test_str= stringify_testvals($list);
		my ($buf_le, $buf_be)= (Userp::Buffer->new_le(\$le), Userp::Buffer->new_be(\$be));
		for (@$list) {
			is( $_->[0], $dec_bits_le->($buf_le, $_->[1]), "decode_bits_le $test_str" )
				or diag _dump_hex($buf_le->[0]);
			is( $_->[0], $dec_bits_be->($buf_be, $_->[1]), "decode_bits_be $test_str" )
				or diag _dump_hex($buf_be->[0]);
		}
	}
	done_testing;
}

sub test_vqty {
	my $class= shift;
	my ($enc_vqty_le, $enc_vqty_be, $dec_vqty_le, $dec_vqty_be)=
		map $class->can($_), qw( buffer_encode_vqty_le buffer_encode_vqty_be buffer_decode_vqty_le buffer_decode_vqty_be );
	my @tests= (
		[ [1], "\x02", "\x01" ],
		[ [0x7F], "\xFE", "\x7F" ],
		[ [0x80], "\x01\x02", "\x80\x80" ],
		[ [0xFF], "\xFD\x03", "\x80\xFF" ],
		[ [0x0FFF], "\xFD\x3F", "\x8F\xFF" ],
		[ [0x3FFF], "\xFD\xFF", "\xBF\xFF" ],
		[ [0x4000], "\x03\x00\x02\x00", "\xC0\x00\x40\x00" ],
		[ [0x1F00_0000], "\x03\x00\x00\xF8", "\xDF\x00\x00\x00" ],
		[ [0x1FFF_FFFF], "\xFB\xFF\xFF\xFF", "\xDF\xFF\xFF\xFF" ],
		[ [0x2000_0000], "\x07\x00\x00\x00\x02\x00\x00\x00", "\xE0\x00\x00\x00\x20\x00\x00\x00" ],
		[ [(bigint(1)<<60)-1], "\xF7\xFF\xFF\xFF\xFF\xFF\xFF\xFF", "\xEF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ],
		[ [(bigint(1)<<60)],   "\x0F\x00\x00\x00\x00\x00\x00\x00\x00\x10", "\xF0\x00\x10\x00\x00\x00\x00\x00\x00\x00" ],
		[ [(bigint(1)<<64)-1], "\x0F\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", "\xF0\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ],
	);
	for (@tests) {
		my ($list, $le, $be)= @$_;
		my $test_str= stringify_testvals($list);
		my ($buf_le, $buf_be)= (Userp::Buffer->new, Userp::Buffer->new);
		for (@$list) {
			$enc_vqty_le->($buf_le, $_);
			$enc_vqty_be->($buf_be, $_);
		}
		is( ${$buf_le->[0]}, $le, "encode_vqty_le $test_str" )
			or diag _dump_hex(${$buf_le->[0]});
		is( ${$buf_be->[0]}, $be, "encode_vqty_be $test_str" )
			or diag _dump_hex(${$buf_be->[0]});
	}
	for (@tests) {
		my ($list, $le, $be)= @$_;
		my $test_str= stringify_testvals($list);
		my ($buf_le, $buf_be)= (Userp::Buffer->new_le(\$le), Userp::Buffer->new_be(\$be));
		for (@$list) {
			# comparing bigint confuses Test2, so stringify
			is( $dec_vqty_le->($buf_le).'', $_.'', "decode_vqty_le $test_str" )
				or diag _dump_hex(${$buf_le->[0]});
			is( $dec_vqty_be->($buf_be).'', $_.'', "decode_vqty_be $test_str" )
				or diag _dump_hex(${$buf_be->[0]});
		}
	}
}

sub test_bitpacking {
	my $class= shift;
	my ($pad_align, $seek_align, $concat_bits_le, $concat_bits_be, $concat_vqty_le, $concat_vqty_be,
		$read_bits_le, $read_bits_be, $read_vqty_le, $read_vqty_be)= map $class->can($_),
		qw( pad_buffer_to_alignment seek_buffer_to_alignment concat_bits_le concat_bits_be
		concat_vqty_le concat_vqty_be read_bits_le read_bits_be read_vqty_le read_vqty_be );
	my @tests= (
		# align, [patterns], known_encoding_le, known_encoding_be
		[ 0, [[1,1]], "\x01", "\x80" ],
		[ 0, [[1,1],[1,3],[1,4]], "\x13", "\x91" ],
		[ 1, [[1,1],[1,3],[1,4],[5,3],[0xFFF,16],[9,4]]],
		[ 2, [[1,1],[1,3],[1,4],[5,3],[0xFFF,16],[9,4]]],
		[ 3, [[1,1],[1,3],[1,4],[5,3],[0xFFF,16],[9,4]]],
		[ 4, [[1,1],[1,3],[1,4],[5,3],[0xFFF,16],[9,4]]],
		[ 5, [[1,1],[1,3],[1,4],[5,3],[0xFFF,16],[9,4]]],
		[ 2, [[1,1],[1,3],[1,7],[0xFFF,16],[9,4]]],
		[ 3, [[1,1],[1,3],[1,7],[0xFFF,16],[9,4]]],
		[ 4, [[1,1],[1,3],[1,7],[0xFFF,16],[9,4]]],
	);
	for (@tests) {
		my ($align, $pattern, $encoding_le, $encoding_be)= @$_;
		subtest 'align='.$align.' '.stringify_testvals($pattern) => sub {
			my %buffers= ( le32 => ['',0], be32 => ['',0], le64 => ['',0], be64 => ['',0] );
			for my $item (@$pattern) {
				$pad_align->(@$_, $align) for values %buffers;
				$concat_bits_le->($buffers{le64}[0], $buffers{le64}[1], @{$item}[1,0]);
				$concat_bits_be->($buffers{be64}[0], $buffers{be64}[1], @{$item}[1,0]);
			}

			# If test has known encodings, verify them
			if (defined $encoding_le) {
				is( $buffers{$_}[0], $encoding_le, "$_ encoding" ) || diag('LE enc: '.main::_dump_hex($buffers{$_}[0]))
					for grep length($buffers{$_}[0]), 'le32','le64';
			}
			if (defined $encoding_be) {
				is( $buffers{$_}[0], $encoding_be, "$_ encoding" ) || diag('BE enc: '.main::_dump_hex($buffers{$_}[0]))
					for grep length($buffers{$_}[0]), 'be32','be64';
			}
		
			# Then verify the same values get decoded
			for (values %buffers) {
				pos($_->[0])= 0;
				$_->[1]= 0;
			}
			for my $item (@$pattern) {
				$seek_align->(@$_, $align) for values %buffers;
				is( $read_bits_le->($buffers{le64}[0], $buffers{le64}[1], $item->[1]), $item->[0], "(64) read le $item->[1]" );
				is( $read_bits_be->($buffers{be64}[0], $buffers{be64}[1], $item->[1]), $item->[0], "(64) read be $item->[1]" );
			}
		};
	}
}

done_testing;
