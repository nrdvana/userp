use Test2::V0;
use Userp::Buffer;

sub main::_dump_hex { join ' ', map sprintf("%02X", $_), unpack 'C*', $_[0] }
sub bigint { Math::BigInt->new(shift) }
sub stringify_testvals { join ' ', map { ref $_ eq 'ARRAY'? '['.join(',', @$_).']' : $_ } @{$_[0]} }

subtest $_ => main->can('test_'.$_)
	for qw( bitpacking intsplice );

sub test_bitpacking {
	my @tests= (
		{
			seq => [
				[ int => 1,1 ],
				[ align => -3 ],
				[ int => 1,1 ],
				[ align => -3 ],
				[ int => 1 ],
			],
			expect_le => "\x03\x02",
			expect_be => "\xC0\x01",
	    },
		{ 
			seq => [
				[ int => 1,1 ],
				[ align => -2 ],
				[ int => 1,1 ],
				[ align => -2 ],
				[ int => 1 ],
			],
			expect_le => "\x05\x02",
			expect_be => "\xA0\x01",
		},
		(map {
			+{seq => [
				[ int => 1,1],
				[ align => $_ ],
				[ int => 1,3],
				[ align => $_ ],
				[ int => 1,4],
				[ align => $_ ],
				[ int => 5,3],
				[ align => $_ ],
				[ int => 0xFFF,16],
				[ align => $_ ],
				[ int => 9,4]
			]}
			} -2,-1,0,1,2
		),
		(map {
			+{seq => [
				[ int => 1,1 ],
				[ align => $_ ],
				[ int => 1,3 ],
				[ align => $_ ],
				[ int => 1,7 ],
				[ align => $_ ],
				[ int => 0xFFF,16 ],
				[ int => 9,4 ]
			]}
			} -2,-1,0,1,2
		),
	);
	my %enc_method = ( int => 'encode_int', align => 'pad_to_alignment' );
	my %dec_method = ( int => sub { $_[0]->decode_int($_[2]) }, align => 'seek_to_alignment' );
	for (@tests) {
		my ($seq, $expect_le, $expect_be)= @{$_}{'seq','expect_le','expect_be'};
		my ($buf_le, $buf_be)= (Userp::Buffer->new_le(undef, 4), Userp::Buffer->new_be(undef, 4));
		my $t_name= compose_name($seq);
		subtest $t_name => sub {
			for my $op (@$seq) {
				my $method= $enc_method{$op->[0]};
				$_->$method(@{$op}[1 .. $#$op]) for $buf_le, $buf_be;
			}
			# If test has known encodings, verify them
			if (defined $expect_le) {
				is( ${ $buf_le->bufref }, $expect_le, "LE encoding" )
					or diag('LE enc: '.main::_dump_hex(${ $buf_le->bufref }));
			}
			if (defined $expect_be) {
				is( ${ $buf_be->bufref }, $expect_be, "BE encoding" )
					or diag('BE enc: '.main::_dump_hex(${ $buf_be->bufref }));
			}
		
			# Then verify the same values get decoded
			$_->seek(0) for $buf_le, $buf_be;
			for my $op (@$seq) {
				my $method= $dec_method{$op->[0]};
				my $ret= $buf_le->$method(@{$op}[1 .. $#$op]);
				is( $ret, $op->[1], "LE $op->[0] => $op->[1]" )
					if $op->[0] eq 'int';
				$ret= $buf_be->$method(@{$op}[1 .. $#$op]);
				is( $ret, $op->[1], "BE $op->[0] => $op->[1]" )
					if $op->[0] eq 'int';
			}
		};
	}
}

sub test_intsplice {
	for my $new_endian (qw( new_le new_be )) {
		subtest $new_endian => sub {
			my $buf= Userp::Buffer->$new_endian;
			$buf->intercept_next_int(sub {
				my ($self, $value, $bits)= @_;
				$self->encode_int($value + 0x10);
			});
			$buf->encode_int(0x10);
			is( length ${$buf->bufref}, 1, 'encoded to single byte' );
			$buf->seek(0);
			is( $buf->decode_int, 0x20, 'decode 0x20' );
			$buf->intercept_next_int(sub { return 0x10 });
			is( $buf->decode_int, 0x10, 'decode 0x10' );
		};
	}
}

sub compose_name {
	my $seq= shift;
	return join ' ', map { $_->[0].'('.join(',',@{$_}[1..$#$_]).')' } @$seq;
}

done_testing;
