use Test2::V0;
use Userp::Buffer;

sub main::_dump_hex { join ' ', map sprintf("%02X", $_), unpack 'C*', $_[0] }
sub bigint { Math::BigInt->new(shift) }
sub stringify_testvals { join ' ', map { ref $_ eq 'ARRAY'? '['.join(',', @$_).']' : $_ } @{$_[0]} }

subtest $_ => main->can('test_'.$_)
	for qw( bitpacking );

sub test_bitpacking {
	my $class= shift;
	my @tests= (
		{
			seq => [
				[ bits => 1,1 ],
				[ align => -3 ],
				[ bits => 1,1 ],
				[ align => -3 ],
				[ vqty => 1 ],
			],
			expect_le => "\x03\x02",
			expect_be => "\xC0\x01",
	    },
		{ 
			seq => [
				[ bits => 1,1 ],
				[ align => -2 ],
				[ bits => 1,1 ],
				[ align => -2 ],
				[ vqty => 1 ],
			],
			expect_le => "\x05\x02",
			expect_be => "\xA0\x01",
		},
		(map {
			+{seq => [
				[ bits => 1,1],
				[ align => $_ ],
				[ bits => 3,1],
				[ align => $_ ],
				[ bits => 4,1],
				[ align => $_ ],
				[ bits => 3,5],
				[ align => $_ ],
				[ bits => 16,0xFFF],
				[ align => $_ ],
				[ bits => 4,9]
			]}
			} -2#,-1,0,1,2
		),
		(map {
			+{seq => [
				[ bits => 1,1 ],
				[ align => $_ ],
				[ bits => 3,1 ],
				[ align => $_ ],
				[ bits => 7,1 ],
				[ align => $_ ],
				[ bits => 16,0xFFF ],
				[ bits => 4,9 ]
			]}
			} -2#,-1,0,1,2
		),
	);
	my %enc_method = ( bits => 'encode_bits', vqty => 'encode_vqty', align => 'pad_to_alignment' );
	my %dec_method = ( bits => 'decode_bits', vqty => 'decode_vqty', align => 'seek_to_alignment' );
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
				my $ret= $buf_le->$method($op->[1]);
				is( $ret, $op->[2], "LE $method $op->[1] = $op->[2]" )
					if defined $op->[2];
				$ret= $buf_be->$method($op->[1]);
				is( $ret, $op->[2], "BE $method $op->[1] = $op->[2]" )
					if defined $op->[2];
			}
		};
	}
}

sub compose_name {
	my $seq= shift;
	return join ' ', map { $_->[0].'('.join(',',@{$_}[1..$#$_]).')' } @$seq;
}

done_testing;
