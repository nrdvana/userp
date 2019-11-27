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

sub twos_minmax {
	my $bits= shift;
	my $max= $bits <= 1? 0 : $bits <= 64? (1<<($bits-1))-1 : Math::BigInt->bone->blsft($bits-1)->bdec;
	return (-1-$max, $max);
}
sub unsigned_max {
	my $bits= shift;
	$bits <= 1? 0 : $bits < 64? (1<<$bits)-1 : $bits == 64? ~0 : Math::BigInt->bone->blsft($bits)->bdec;
}

sub sign_extend {
	my ($n, $bits)= @_;
	use integer;
	return $n unless $n >> ($bits-1); # don't mess with positive values
	return $n+0 if $bits == 64; # addition in scope of 'use integer' becomes signed.
	return (-1 << $bits) | $n if $bits < 64;
	return Math::BigInt->new(-1)->blsft($bits) | $n;
}

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

sub concat_int_le {
	# ($buffer, $bits, $value)
	my $n= ($_[1]+7)>>3;
	$_[0] .= $n == 1? pack 'C', $_[2]
		: $n == 2? pack 'S<', $_[2]
		: $n == 4? pack 'L<', $_[2]
		: $n == 8? pack 'Q<', $_[2]
		: $n < 8? substr(pack('Q<', $_[2]), 0, $n)
		: reverse substr(Math::BigInt->new($_[2])->as_bytes, -$n);
}

sub concat_int_be {
	# ($buffer, $bits, $value)
	my $n= ($_[1]+7)>>3;
	$_[0] .= $n == 1? pack 'C', $_[2]
		: $n == 2? pack 'S>', $_[2]
		: $n == 4? pack 'L>', $_[2]
		: $n == 8? pack 'Q>', $_[2]
		: $n < 8? substr(pack('Q>', $_[2]), -$n)
		: substr(Math::BigInt->new($_[2])->as_bytes, -$n);
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

sub seek_buffer_to_alignment {
	#my ($buffer, $bitpos, $align)= @_;
	my $mask= (1 << $_[2]) - 1;
	$_[1]= ($_[1] + $mask) & ~$mask;
}

sub read_bits_le {
	# (buffer, bitpos, int_bits)
	return undef unless (($_[1]+$_[2]+7)>>3) <= length $_[0];
	my $bit_remainder= $_[1] & 7;
	my $bit_read= $bit_remainder + $_[2];
	my $bytes= substr($_[0], $_[1]>>3, ($bit_read+7)>>3);
	# clear lowest bits of final byte, if needed
	substr($bytes,-1)= chr( (0xFF >> (8-($bit_read&7))) & ord substr($bytes,-1) )
		if $bit_read & 7;
	$_[1] += $_[2];
	my $v;
	if ($bit_read <= 64) {
		$v= unpack('Q<', $bytes."\0\0\0\0\0\0\0\0");
		$v >>= $bit_remainder;
	}
	else {
		$v= Math::BigInt->from_bytes(reverse $bytes);
		$v->brsft($bit_remainder) if $bit_remainder;
	}
	return $v;
}

sub read_bits_be {
	# (buffer, bitpos, int_bits)
	return undef unless (($_[1]+$_[2]+7)>>3) <= length $_[0];
	my $bit_remainder= $_[1] & 7;
	my $bit_read= $bit_remainder + $_[2];
	my $bytes= substr($_[0], $_[1]>>3, ($bit_read+7)>>3);
	# clear highest bits of first byte, if needed
	substr($bytes,0,1)= chr( (0xFF >> $bit_remainder) & ord substr($bytes,0,1) )
		if $bit_remainder;
	$_[1] += $_[2];
	my $v;
	if ($bit_read <= 64) {
		$v= unpack('Q>', $bytes."\0\0\0\0\0\0\0\0");
		$v >>= 64 - $bit_read;
	}
	else {
		$v= Math::BigInt->from_bytes($bytes);
		$v->brsft(8 - ($bit_read & 7)) if $bit_read & 7;
	}
	return $v;
}

sub read_int_le {
	# (buffer, int_bits)
	my $n= ($_[1]+7)>>3;
	pos($_[0])+$n <= length($_[0])
		or die Userp::Error::EOF->for_buf($_[0], $n, 'int-'.$n);
	my $buf= substr($_[0], (pos($_[0])+=1)-1, $n);
	return ord $buf if $n == 1;
	return unpack 'S<', $buf if $n == 2;
	return unpack 'L<', $buf if $n == 4;
	return unpack 'Q<', $buf if $n == 8;
	return unpack('L<', $buf."\0")&0xFFFFFF if $n == 3;
	return unpack('Q<', $buf."\0\0\0")&((1 << $_[1]) - 1) if $n < 8;
	return Math::BigInt->from_bytes(reverse $buf);
}

sub read_int_be {
	# (buffer, int_bits)
	my $n= ($_[1]+7)>>3;
	pos($_[0])+$n <= length($_[0])
		or die Userp::Error::EOF->for_buf($_[0], $n, 'int-'.$n);
	my $buf= substr($_[0], (pos($_[0])+=1)-1, $n);
	return ord $buf if $n == 1;
	return unpack 'S>', $buf if $n == 2;
	return unpack 'L>', $buf if $n == 4;
	return unpack 'Q>', $buf if $n == 8;
	return unpack('L>', "\0".$buf)&0xFFFFFF if $n == 3;
	return unpack('Q>', "\0\0\0".$buf)&((1 << $_[1]) - 1) if $n < 8;
	return Math::BigInt->from_bytes($buf);
}

sub read_vqty_le {
	#my ($buffer)= @_;
	pos($_[0]) < length($_[0])
		or die Userp::Error::EOF->for_buf($_[0], 1, 'vqty');
	my $switch= ord substr($_[0], pos($_[0]), 1);
	if (!($switch & 1)) {
		++pos($_[0]);
		return $switch >> 1;
	}
	elsif (!($switch & 2)) {
		pos($_[0])+2 <= length($_[0])
			or die Userp::Error::EOF->for_buf($_[0], 2, 'vqty-2');
		return unpack('S<', substr($_[0], (pos($_[0])+=2)-2, 2)) >> 2;
	}
	elsif (!($switch & 4)) {
		pos($_[0])+4 <= length($_[0])
			or die Userp::Error::EOF->for_buf($_[0], 4, 'vqty-4');
		return unpack('L<', substr($_[0], (pos($_[0])+=4)-4, 4)) >> 3;
	}
	elsif ($switch != 0xFF) {
		pos($_[0])+8 <= length($_[0])
			or die Userp::Error::EOF->for_buf($_[0], 8, 'vqty-8');
		return unpack('Q<', substr($_[0], (pos($_[0])+=8)-8, 8)) >> 3;
	} else {
		pos($_[0])+10 <= length($_[0])
			or die Userp::Error::EOF->for_buf($_[0], 10, 'vqty-N');
		my $n= ord substr($_[0], (pos($_[0])+=2)-1, 1);
		$n= unpack('S<', substr($_[0], (pos($_[0])+=2)-2, 2)) if $n == 0xFF;
		$n < 0xFFFF or die Userp::Error::ImplLimit->new(message => "Refusing to decode ludicrously large integer value");
		$n += 8;
		pos($_[0]) + $n <= length($_[0])
			or die Userp::Error::EOF->for_buf($_[0], $n, 'vqty-'.$n);
		return Math::BigInt->from_bytes(reverse substr($_[0], (pos($_[0])+=$n)-$n, $n));
	}
}

sub read_vqty_be {
	#my ($buffer)= @_;
	pos($_[0]) < length($_[0])
		or die Userp::Error::EOF->for_buf($_[0], 1, 'vqty');
	my $switch= ord substr($_[0], pos($_[0]), 1);
	if ($switch < 0x80) {
		++pos($_[0]);
		return $switch;
	}
	elsif ($switch < 0xC0) {
		pos($_[0])+2 <= length($_[0])
			or die Userp::Error::EOF->for_buf($_[0], 2, 'vqty-2');
		return unpack('S>', substr($_[0], (pos($_[0])+=2)-2, 2)) & 0x3FFF;
	}
	elsif ($switch < 0xE0) {
		pos($_[0])+4 <= length($_[0])
			or die Userp::Error::EOF->for_buf($_[0], 4, 'vqty-4');
		return unpack('L>', substr($_[0], (pos($_[0])+=4)-4, 4)) & 0x1FFF_FFFF;
	}
	elsif ($switch < 0xFF) {
		pos($_[0])+8 <= length($_[0])
			or die Userp::Error::EOF->for_buf($_[0], 8, 'vqty-8');
		return unpack('Q>', substr($_[0], (pos($_[0])+=8)-8, 8)) & 0x0FFF_FFFF_FFFF_FFFF;
	}
	else {
		pos($_[0])+10 <= length($_[0])
			or die Userp::Error::EOF->for_buf($_[0], 10, 'vqty-N');
		my $n= ord substr($_[0], (pos($_[0])+=2)-1, 1);
		$n= unpack('S>', substr($_[0], (pos($_[0])+=2)-2, 2)) if $n == 0xFF;
		$n < 0xFFFF or die Userp::Error::ImplLimit->new(message => "Refusing to decode ludicrously large integer value");
		$n += 8;
		pos($_[0]) + $n <= length($_[0])
			or die Userp::Error::EOF->for_buf($_[0], $n, 'vqty-'.$n);
		return Math::BigInt->from_bytes(substr($_[0], (pos($_[0])+=$n)-$n, $n));
	}
}

sub read_symbol_le {
	#my ($buffer)= @_;
	my $symbol_id= &read_vqty_le;
	my $suffix;
	if ($symbol_id & 1) {
		my $len= &read_vqty_le;
		$len > 0
			or Userp::Error::Domain->throw({ value => $len, min => 1 });
		length($_[0]) - pos($_[0]) >= $len
			or Userp::Error::EOF->throw({ pos => pos($_[0]), buffer => $_[0], len => $len });
		$suffix= substr($_[0], pos($_[0]), $len);
		pos($_[0]) += $len;
	}
	return ($symbol_id>>1, $suffix);
}

sub read_symbol_be {
	#my ($buffer)= @_;
	my $symbol_id= &read_vqty_be;
	my $suffix;
	if ($symbol_id & 1) {
		my $len= &read_vqty_be;
		$len > 0
			or die Userp::Error::Domain->new({ value => $len, min => 1 });
		length($_[0]) - pos($_[0]) >= $len
			or die Userp::Error::EOF->for_buf($_[0], $len, 'symbol suffix');
		$suffix= substr($_[0], pos($_[0]), $len);
		pos($_[0]) += $len;
	}
	return ($symbol_id>>1, $suffix);
}

1;
