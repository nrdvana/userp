use Test2::V0;
use Data::Printer;

sub main::_dump_hex { join ' ', map sprintf("%02X", $_), unpack 'C*', $_[0] }

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
	subtest bitpacking => sub { test_bitpacking($class) };
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
