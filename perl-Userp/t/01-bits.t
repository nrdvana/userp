use Test2::V0;
use Data::Printer;

sub main::_dump_hex { join ' ', map sprintf("%02X", $_), unpack 'C*', $_[0] }
sub bigint { Math::BigInt->new(shift) }

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
	subtest pack_bits => sub { test_pack_bits($class) };
	subtest bits => sub { test_bits($class) };
	subtest ints => sub { test_ints($class) };
	subtest vqty => sub { test_vqty($class) };
	#subtest bitpacking => sub { test_bitpacking($class) };
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
	my ($concat_bits_le, $concat_bits_be, $read_bits_le, $read_bits_be)=
		map $class->can($_), qw( concat_bits_le concat_bits_be read_bits_le read_bits_be );
	my @tests= (
		# value_and_bits_list, encoding_le, encoding_be
		[ [ [1,1], [1,1] ], "\x03", "\xC0" ],
		[ [ [1,3], [1,9] ], "\x09\x00", "\x20\x10" ],
		[ [ [7,3], [7,3], [3,2], [1,1] ], "\xFF\x01", "\xFF\x80" ],
		[ [ [7,3], [7,3], [7,3] ], "\xFF\x01", "\xFF\x80" ],
	);
	for (@tests) {
		my ($list, $le, $be)= @$_;
		my $test_str= join(' ', map '['.join(',', @$_).']', @$list);
		my ($buf_le, $bitofs_le, $buf_be, $bitofs_be)= ('', 0, '', 0);
		for (@$list) {
			$concat_bits_le->($buf_le, $bitofs_le, $_->[1], $_->[0]);
			$concat_bits_be->($buf_be, $bitofs_be, $_->[1], $_->[0]);
		}
		is( $buf_le, $le, "concat_bits_le $test_str" );
		is( $buf_be, $be, "concat_bits_be $test_str" );
	}
	for (@tests) {
		my ($list, $le, $be)= @$_;
		my $test_str= join(' ', map '['.join(',', @$_).']', @$list);
		my ($buf_le, $bitofs_le, $buf_be, $bitofs_be)= ($le, 0, $be, 0);
		for (@$list) {
			is( $_->[0], $read_bits_le->($buf_le, $bitofs_le, $_->[1]), "read_bits_le $test_str" );
			is( $_->[0], $read_bits_be->($buf_be, $bitofs_be, $_->[1]), "read_bits_be $test_str" );
		}
	}
	done_testing;
}

sub test_ints {
	my $class= shift;
	my ($concat_int_le, $concat_int_be, $read_int_le, $read_int_be)=
		map $class->can($_), qw( concat_int_le concat_int_be read_int_le read_int_be );
	my @tests= (
		[ [[1,1],[7,3],[5,3],[0xFFF,16],[9,4]], "\x01\x07\x05\xFF\x0F\x09", "\x01\x07\x05\x0F\xFF\x09"],
	);
	for (@tests) {
		my ($list, $le, $be)= @$_;
		my $test_str= join(' ', map '['.join(',', @$_).']', @$list);
		my ($buf_le, $buf_be)= ('', '');
		for (@$list) {
			$concat_int_le->($buf_le, $_->[1], $_->[0]);
			$concat_int_be->($buf_be, $_->[1], $_->[0]);
		}
		is( $buf_le, $le, "concat_int_le $test_str" )
			or diag _dump_hex($buf_le);
		is( $buf_be, $be, "concat_int_be $test_str" )
			or diag _dump_hex($buf_be);
	}
	for (@tests) {
		my ($list, $le, $be)= @$_;
		my $test_str= join(' ', map '['.join(',', @$_).']', @$list);
		my ($buf_le, $buf_be)= ($le, $be);
		for (@$list) {
			is( $read_int_le->($buf_le, $_->[1]), $_->[0], "read_int_le $test_str" );
			is( $read_int_be->($buf_be, $_->[1]), $_->[0], "read_int_be $test_str" );
		}
	}
}

sub test_vqty {
	my $class= shift;
	my ($concat_vqty_le, $concat_vqty_be, $read_vqty_le, $read_vqty_be)=
		map $class->can($_), qw( concat_vqty_le concat_vqty_be read_vqty_le read_vqty_be );
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
		my $test_str= join(' ', @$list);
		my ($buf_le, $buf_be)= ('', '');
		for (@$list) {
			$concat_vqty_le->($buf_le, $_);
			$concat_vqty_be->($buf_be, $_);
		}
		is( $buf_le, $le, "concat_vqty_le $test_str" )
			or diag _dump_hex($buf_le);
		is( $buf_be, $be, "concat_vqty_be $test_str" )
			or diag _dump_hex($buf_be);
	}
	for (@tests) {
		my ($list, $le, $be)= @$_;
		my $test_str= join(' ', @$list);
		my ($buf_le, $buf_be)= ($le, $be);
		pos($buf_le)= 0; pos($buf_be)= 0;
		for (@$list) {
			# comparing bigint confuses Test2, so stringify
			is( $read_vqty_le->($buf_le).'', $_.'', "read_vqty_le $test_str" );
			is( $read_vqty_be->($buf_be).'', $_.'', "read_vqty_be $test_str" );
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
		[ 3, [[1]], "\x02", "\x01" ],
		[ 2, [[1],[1,3],[1,7],[5],[0xFFF,16],[9,4]]],
		[ 3, [[1],[1,3],[1,7],[5],[0xFFF,16],[9,4]]],
		[ 4, [[1],[1,3],[1,7],[5],[0xFFF,16],[9,4]]],
	);
	for (@tests) {
		my ($align, $pattern, $encoding_le, $encoding_be)= @$_;
		subtest 'align='.$align.' '.join(',', map { '['.join(',',@$_).']' } @$pattern) => sub {
			my %buffers= ( le32 => ['',0], be32 => ['',0], le64 => ['',0], be64 => ['',0] );
			for my $item (@$pattern) {
				if (@$item > 1) {
					$pad_align->(@$_, $align) for values %buffers;
					$concat_bits_le->($buffers{le64}[0], $buffers{le64}[1], @{$item}[1,0]);
					$concat_bits_be->($buffers{be64}[0], $buffers{be64}[1], @{$item}[1,0]);
				} else {
					$pad_align->(@$_, $align > 3? $align : 3) for values %buffers;
					$concat_vqty_le->($buffers{le64}[0], $item->[0]);
					$concat_vqty_be->($buffers{be64}[0], $item->[0]);
				}
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
				if (@$item > 1) {
					is( $read_bits_le->($buffers{le64}[0], $buffers{le64}[1], $item->[1]), $item->[0], "(64) read le $item->[1]" );
					is( $read_bits_be->($buffers{be64}[0], $buffers{be64}[1], $item->[1]), $item->[0], "(64) read be $item->[1]" );
				} else {
					is( $read_vqty_le->($buffers{le64}[0], $buffers{le64}[1]), $item->[0], "(64) read le vqty" );
					is( $read_vqty_be->($buffers{be64}[0], $buffers{be64}[1]), $item->[0], "(64) read be vqty" );
				}
			}
		};
	}
}

done_testing;
