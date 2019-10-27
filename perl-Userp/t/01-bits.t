use Test2::V0;
use Data::Printer;
#use Userp::PP::Bits32;
use Userp::PP::Bits64;

sub main::_dump_hex { join ' ', map sprintf("%02X", $_), unpack 'C*', $_[0] }

my @tests= (
	# align, [patterns], known_encoding_le, known_encoding_be
	[ 0, [[1,1]], "\x01", "\x80" ],
	[ 0, [[1,1],[1,3],[1,4]], "\x13", "\x91" ],
);
for (@tests) {
	my ($align, $pattern, $encoding_le, $encoding_be)= @$_;
	subtest 'align='.$align.' '.join(',', map { '['.join(',',@$_).']' } @$pattern) => sub {
		my %buffers= ( le32 => ['',0], be32 => ['',0], le64 => ['',0], be64 => ['',0] );
		for my $item (@$pattern) {
			Userp::PP::Bits64::pad_buffer_to_alignment(@$_, $align) for values %buffers;
			if (@$item > 1) {
				Userp::PP::Bits64::concat_bits_le($buffers{le64}[0], $buffers{le64}[1], @{$item}[1,0]);
				Userp::PP::Bits64::concat_bits_be($buffers{be64}[0], $buffers{be64}[1], @{$item}[1,0]);
			} else {
				Userp::PP::Bits64::concat_vqty_le($buffers{le64}[0], $buffers{le64}[1], $item->[0]);
				Userp::PP::Bits64::concat_vqty_be($buffers{be64}[0], $buffers{be64}[1], $item->[0]);
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
			Userp::PP::Bits64::seek_buffer_to_alignment(@$_, $align) for values %buffers;
			if (@$item > 1) {
				is( Userp::PP::Bits64::read_bits_le($buffers{le64}[0], $buffers{le64}[1], $item->[1]), $item->[0], "(64) read le $item->[1]" );
				is( Userp::PP::Bits64::read_bits_be($buffers{be64}[0], $buffers{be64}[1], $item->[1]), $item->[0], "(64) read be $item->[1]" );
			} else {
				is( Userp::PP::Bits64::read_vqty_le($buffers{le64}[0], $buffers{le64}[1]), $item->[0], "(64) read le vqty" );
				is( Userp::PP::Bits64::read_vqty_be($buffers{be64}[0], $buffers{be64}[1]), $item->[0], "(64) read be vqty" );
			}
		}
	};
}

done_testing;
