package Userp::PP::Bits64;
use Config;
use strict;
use warnings;
no warnings 'portable';
use Math::BigInt;
use Carp;
BEGIN { eval "pack('Q<',1)" or die "pack('Q') not supported on this perl.  Use Bits32.pm instead." }

use constant {
	scalar_bit_size => $Config{uvsize},
	pack_bit_size => 64,
};

sub pack_bits_le {
	# (bits, value)
	my $enc= ref($_[1])? reverse($_[1]->as_bytes) : pack('Q<',$_[1]);
	my $byte_n= ($_[0]+7)>>3;
	$enc.= ("\0"x$byte_n) if length($enc) < $byte_n;
	substr($enc, 0, $byte_n);
}

sub pack_bits_be {
	# (bits, value)
	my $v= $_[1];
	if ($_[0] & 7) {
		$v= Math::BigInt->new($v) if $_[0] > 64 && !ref $v;
		$v <<= 8 - ($_[0] & 7);
	}
	my $enc= ref($v)? $v->as_bytes : pack('Q>',$v);
	my $byte_n= ($_[0]+7)>>3;
	$enc= ("\0"x$byte_n).$enc if length($enc) < $byte_n;
	substr($enc, -$byte_n);
}

sub pad_buffer_to_alignment {
	#my ($buffer, $bitpos, $align)= @_;
	if ($_[2] == 3) { # byte-align
		$_[1]= 0;
	}
	elsif ($_[2] > 3) {
		my $n_bytes= 1 << ($_[2] - 3);
		my $remainder= length($_[0]) & ($n_bytes - 1);
		$_[0] .= "\0"x($n_bytes - $remainder) if $remainder;
		$_[1]= 0;
	}
	elsif ($_[2] > 0 && $_[1]) {
		$_[1]= ($_[1] + 1) & 6 if $_[2] == 1;
		$_[1]= ($_[1] + 3) & 4 if $_[2] == 2;
	}
}

sub concat_bits_le {
	# (buffer, prev_bits, bits, value)
	if ($_[1]) {
		my $v= ($_[1] + $_[2] > 64)? Math::BigInt->new($_[3]) : $_[3];
		$v <<= $_[1];
		$v |= ord substr($_[0],-1);
		substr($_[0], -1)= pack_bits_le($_[1]+$_[2], $v);
		$_[1]= ($_[1] + $_[2]) & 7;
	} else {
		$_[0] .= pack_bits_le($_[2], $_[3]);
		$_[1]= $_[2] & 7;
	}
}

sub concat_bits_be {
	# (buffer, prev_bits, bits, value)
	if ($_[1]) {
		my $prev= ord substr($_[0],-1);
		$prev= Math::BigInt->new($prev) if $_[1] + $_[2] > 64;
		$prev <<= $_[2] - (8-$_[1]);
		substr($_[0], -1)= pack_bits_be($_[1]+$_[2], $prev|$_[3]);
		$_[1]= ($_[1] + $_[2]) & 7;
	}
	else {
		$_[0] .= pack_bits_be($_[2], $_[3]);
		$_[1]= $_[2] & 7;
	}
}

sub concat_vqty_le {
	#my ($buffer, $value)= @_;
	my $value= $_[1];
	$_[0] .= $value < 0x80? pack('C',$value<<1)
		: $value < 0x4000? pack('S<',($value<<2)+1)
		: $value < 0x20000000? pack('L<',($value<<3)+3)
		: $value < 0x1FFFFFFF_FFFFFFFF? pack('Q<',($value<<3)+7)
		: do {
			my $bytes= ref($value)? reverse($value->as_bytes) : pack('Q<',$value);
			my $bcnt= length $bytes;
			$bcnt >= 8 or croak "BUG: bigint wasn't 8 bytes long";
			$bcnt -= 8;
			$bcnt < 0xFFFF or croak "Refusing to encode ludicrously large integer value";
			($bcnt < 0xFF? "\xFF".pack('C', $bcnt) : "\xFF\xFF".pack('S<', $bcnt)).$bytes
		};
}

sub concat_vqty_be {
	#my ($buffer, $value)= @_;
	my $value= $_[1];
	$_[0] .= $value < 0x80? pack('C',$value)
		: $value < 0x4000? pack('S>',0x8000 | $value)
		: $value < 0x20000000? pack('L>',0xC000_0000 | $value)
		: $value < 0x1000_0000_0000_0000? pack('Q>',0xE000_0000_0000_0000 | $value)
		: do {
			my $bytes= ref($value)? $value->as_bytes : pack('Q>',$value);
			my $bcnt= length $bytes;
			$bcnt >= 8 or croak "BUG: bigint wasn't 8 bytes long";
			$bcnt -= 8;
			$bcnt < 0xFFFF or croak "Refusing to encode ludicrously large integer value";
			($bcnt < 0xFF? "\xFF".pack('C', $bcnt) : "\xFF\xFF".pack('S>', $bcnt)).$bytes
		};
}

1;
