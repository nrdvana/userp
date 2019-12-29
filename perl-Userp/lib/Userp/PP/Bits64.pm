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

my @_bitlen= ( 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4 );
sub bitlen {
	my ($val)= @_;
	return 0 unless $val;
	return length($val->as_bin)-2 if ref $val; # TODO: use smarter algorithm
	my $len= 0;
	($val >>= 32, $len += 32) if $val >> 32;
	($val >>= 16, $len += 16) if $val >> 16;
	($val >>=  8, $len +=  8) if $val >>  8;
	($val >>=  4, $len +=  4) if $val >>  4;
	return $len + $_bitlen[$val]
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
	# (buffer, bit_pos, bits, value)
	# If bit_pos is a byte boundary and bit_count is a whole number of bytes,
	# use the simpler byte-based code:
	my $bit_ofs= $_[1] & 7;
	if (!$bit_ofs && !($_[2] & 7)) {
		my $n= $_[2] >> 3;
		$_[0] .= $n == 1? pack 'C', $_[3]
			: $n == 2? pack 'S<', $_[3]
			: $n == 4? pack 'L<', $_[3]
			: $n == 8? pack 'Q<', $_[3]
			: $n < 8? substr(pack('Q<', $_[3]), 0, $n)
			: scalar reverse substr(Math::BigInt->new($_[3])->as_bytes, -$n);
	}
	elsif (!$bit_ofs) {
		$_[0] .= pack_bits_le($_[2], $_[3]);
	}
	else {
		my $wbits= $bit_ofs + $_[2];
		my $v= ($wbits > 64)? Math::BigInt->new($_[3]) : $_[3];
		$v <<= $bit_ofs;
		$v |= ord substr($_[0],-1);
		substr($_[0], -1)= pack_bits_le($wbits, $v);
	}
	$_[1] += $_[2];
}

sub concat_bits_be {
	# (buffer, bit_pos, bits, value)
	# If bit_pos is a byte boundary and bit_count is a whole number of bytes,
	# use the simpler byte-based code:
	my $bit_ofs= $_[1] & 7;
	if (!$bit_ofs && !($_[2] & 7)) {
		my $n= $_[2] >> 3;
		$_[0] .= $n == 1? pack 'C', $_[3]
			: $n == 2? pack 'S>', $_[3]
			: $n == 4? pack 'L>', $_[3]
			: $n == 8? pack 'Q>', $_[3]
			: $n < 8? substr(pack('Q>', $_[3]), -$n)
			: substr(Math::BigInt->new($_[3])->as_bytes, -$n);
	}
	elsif (!$bit_ofs) {
		$_[0] .= pack_bits_be($_[2], $_[3]);
	}
	else {
		my $wbits= $bit_ofs + $_[2];
		my $prev= ord substr($_[0],-1);
		$prev= Math::BigInt->new($prev) if $wbits > 64;
		$prev <<= $_[2] - (8-$bit_ofs);
		substr($_[0], -1)= pack_bits_be($wbits, $prev|$_[3]);
	}
	$_[1] += $_[2];
}

sub concat_vqty_le {
	#my ($buffer, $bitpos, $value)= @_;
	my $value= $_[2];
	$_[0] .= $value < 0x80? pack('C',$value<<1)
		: $value < 0x4000? pack('S<',($value<<2)+1)
		: $value < 0x2000_0000? pack('L<',($value<<3)+3)
		: $value < 0x1000_0000_0000_0000? pack('Q<',($value<<4)+7)
		: do {
			my $bytes= ref($value)? reverse($value->as_bytes) : pack('Q<',$value);
			# needs to be a multiple of 4 bytes
			$bytes .= "\0" x (4 - (length($bytes) & 3))
				if length($bytes) & 3;
			my $n= (length($bytes) >> 2) - 2;
			$n < 0? croak "BUG: bigint wasn't 8 bytes long"
			: $n < 0xFFF? pack('S<', ($n << 4) | 0xF) . $bytes
			: $n < 0xFFFF_FFFF? "\xFF\xFF" . pack('L<', $n) . $bytes
			: Userp::Error::ImplLimit->throw(message => "Refusing to encode ludicrously large integer value");
		};
	$_[1]= length($_[0])<<3;
}

sub concat_vqty_be {
	#my ($buffer, $bitpos, $value)= @_;
	my $value= $_[2];
	$_[0] .= $value < 0x80? pack('C',$value)
		: $value < 0x4000? pack('S>',0x8000 | $value)
		: $value < 0x2000_0000? pack('L>',0xC000_0000 | $value)
		: $value < 0x1000_0000_0000_0000? pack('Q>',0xE000_0000_0000_0000 | $value)
		: do {
			my $bytes= ref($value)? $value->as_bytes : pack('Q>',$value);
			# needs to be a multiple of 4 bytes
			$bytes= ("\0" x (4 - (length($bytes) & 3))) . $bytes
				if length($bytes) & 3;
			my $n= (length($bytes) >> 2) - 2;
			$n < 0? croak "BUG: bigint wasn't 8 bytes long"
			: $n < 0xFFF? pack('S>', 0xF000 | $n).$bytes
			: $n < 0xFFFF_FFFF? "\xFF\xFF".pack('L>', $n).$bytes
			: Userp::Error::ImplLimit->throw(message => "Refusing to encode ludicrously large integer value");
		};
	$_[1]= length($_[0])<<3;
}

sub seek_buffer_to_alignment {
	#my ($buffer, $bitpos, $align)= @_;
	my $mask= (1 << $_[2]) - 1;
	$_[1]= ($_[1] + $mask) & ~$mask;
}

sub read_bits_le {
	# (buffer, bit_pos, bit_count)
	# Ensure we have this many bits available
	(($_[1]+$_[2]+7)>>3) <= length $_[0]
		or die Userp::Error::EOF->for_buf_bits($_[0], $_[1], $_[2], 'int');
	# If bit_pos is a byte boundary and bit_count is a whole number of bytes,
	# use the simpler byte-based code:
	if (!($_[1] & 7) && !($_[2] & 7)) {
		my $buf= substr($_[0], $_[1] >> 3, $_[2] >> 3);
		$_[1]+= $_[2];
		return unpack('Q<', $buf."\0\0\0\0\0\0\0\0") if length $buf <= 8;
		return Math::BigInt->from_bytes(scalar reverse $buf);
	}
	else {
		my $bit_remainder= $_[1] & 7;
		my $bit_read= $bit_remainder + $_[2]; # total bits being read including remainder
		my $buf= substr($_[0], $_[1]>>3, ($bit_read+7)>>3);
		# clear highest bits of final byte, if needed
		substr($buf,-1)= chr( (0xFF >> (8-($bit_read&7))) & ord substr($buf,-1) )
			if $bit_read & 7;
		$_[1] += $_[2];
		if ($bit_read <= 64) {
			my $v= unpack('Q<', $buf."\0\0\0\0\0\0\0\0");
			return $v >>= $bit_remainder;
		}
		else {
			my $v= Math::BigInt->from_bytes(scalar reverse $buf);
			$v->brsft($bit_remainder) if $bit_remainder;
			return $v;
		}
	}
}

sub read_bits_be {
	# (buffer, bit_pos, bit_count)
	# Ensure we have this many bits available
	(($_[1]+$_[2]+7)>>3) <= length $_[0]
		or die Userp::Error::EOF->for_buf_bits($_[0], $_[1], $_[2], 'int');
	# If bit_pos is a byte boundary and bit_count is a whole number of bytes,
	# use the simpler byte-based code:
	if (!($_[1] & 7) && !($_[2] & 7)) {
		my $buf= substr($_[0], $_[1] >> 3, $_[2] >> 3);
		$_[1]+= $_[2];
		return unpack 'C', $buf if $_[2] == 8;
		return unpack 'S>', $buf if $_[2] == 16;
		return unpack 'L>', $buf if $_[2] == 32;
		return unpack 'Q>', $buf if $_[2] == 64;
		return unpack('Q>', ("\0" x (8-length $buf)) . $buf) & ((1 << $_[2]) - 1)
			if $_[2] < 64;
		return Math::BigInt->from_bytes($buf);
	}
	else {
		my $bit_remainder= $_[1] & 7;
		my $bit_read= $bit_remainder + $_[2];
		my $buf= substr($_[0], $_[1] >> 3, ($bit_read+7) >> 3);
		# clear highest bits of first byte, if needed
		substr($buf,0,1)= chr( (0xFF >> $bit_remainder) & ord substr($buf,0,1) )
			if $bit_remainder;
		$_[1] += $_[2];
		if ($bit_read <= 64) {
			return unpack('Q>', $buf."\0\0\0\0\0\0\0\0") >> (64 - $bit_read);
		}
		else {
			my $v= Math::BigInt->from_bytes($buf);
			$v->brsft(8 - ($bit_read & 7)) if $bit_read & 7;
			return $v;
		}
	}
}

sub read_vqty_le {
	#my ($buffer, $bitpos)= @_;
	my $pos= ($_[1]+7)>>3;
	$pos < length $_[0]
		or die Userp::Error::EOF->for_buf($_[0], $pos, 1, 'vqty');
	my $switch= ord substr($_[0], $pos, 1);
	my $v;
	if (!($switch & 1)) {
		++$pos;
		$v= $switch >> 1;
	}
	elsif (!($switch & 2)) {
		$pos+2 <= length $_[0]
			or die Userp::Error::EOF->for_buf($_[0], $pos, 2, 'vqty-2');
		$v= unpack('S<', substr($_[0], $pos, 2)) >> 2;
		$pos+= 2;
	}
	elsif (!($switch & 4)) {
		$pos+4 <= length $_[0]
			or die Userp::Error::EOF->for_buf($_[0], $pos, 4, 'vqty-4');
		$v= unpack('L<', substr($_[0], $pos, 4)) >> 3;
		$pos+= 4;
	}
	elsif (!($switch & 8)) {
		$pos+8 <= length $_[0]
			or die Userp::Error::EOF->for_buf($_[0], $pos, 8, 'vqty-8');
		$v= unpack('Q<', substr($_[0], $pos, 8)) >> 4;
		$pos+= 8;
	} else {
		$pos+6 <= length $_[0]
			or die Userp::Error::EOF->for_buf($_[0], $pos, 6, 'vqty-N');
		my $n= unpack('S<', substr($_[0], $pos, 2));
		$pos+= 2;
		if ($n == 0xFFFF) {
			$n= unpack('L<', substr($_[0], $pos, 4));
			$pos+= 4;
			$n < 0xFFFF_FFFF or Userp::Error::ImplLimit->throw(message => "Refusing to decode ludicrously large integer value");
		} else {
			$n >>= 4;
		}
		# number of bytes is 8 + $n * 4
		$n= ($n << 2) + 8;
		$pos + $n <= length $_[0]
			or die Userp::Error::EOF->for_buf($_[0], $pos, $n, 'vqty-'.$n);
		$v= Math::BigInt->from_bytes(scalar reverse substr($_[0], $pos, $n));
		$pos+= $n;
	}
	$_[1]= $pos << 3;
	return $v;
}

sub read_vqty_be {
	#my ($buffer, $bitpos)= @_;
	my $pos= ($_[1]+7)>>3;
	$pos < length $_[0]
		or die Userp::Error::EOF->for_buf($_[0], $pos, 1, 'vqty');
	my $switch= ord substr($_[0], $pos, 1);
	my $v;
	if ($switch < 0x80) {
		++$pos;
		$v= $switch;
	}
	elsif ($switch < 0xC0) {
		$pos+2 <= length $_[0]
			or die Userp::Error::EOF->for_buf($_[0], $pos, 2, 'vqty-2');
		$v= unpack('S>', substr($_[0], $pos, 2)) & 0x3FFF;
		$pos+= 2;
	}
	elsif ($switch < 0xE0) {
		$pos+4 <= length $_[0]
			or die Userp::Error::EOF->for_buf($_[0], $pos, 4, 'vqty-4');
		$v= unpack('L>', substr($_[0], $pos, 4)) & 0x1FFF_FFFF;
		$pos+= 4;
	}
	elsif ($switch < 0xF0) {
		$pos+8 <= length $_[0]
			or die Userp::Error::EOF->for_buf($_[0], $pos, 8, 'vqty-8');
		$v= unpack('Q>', substr($_[0], $pos, 8)) & 0x0FFF_FFFF_FFFF_FFFF;
		$pos+= 8;
	}
	else {
		$pos+6 <= length $_[0]
			or die Userp::Error::EOF->for_buf($_[0], $pos, 6, 'vqty-N');
		my $n= unpack('S>', substr($_[0], $pos, 2));
		$pos+= 2;
		if ($n == 0xFFFF) {
			$n= unpack('L>', substr($_[0], $pos, 4));
			$pos+= 4;
			$n < 0xFFFF_FFFF or Userp::Error::ImplLimit->throw(message => "Refusing to decode ludicrously large integer value");
		} else {
			$n &= 0xFFF;
		}
		# number of bytes is 8 + $n * 4
		$n= ($n << 2) + 8;
		$pos + $n <= length $_[0]
			or die Userp::Error::EOF->for_buf($_[0], $pos, $n, 'vqty-'.$n);
		$v= Math::BigInt->from_bytes(substr($_[0], $pos, $n));
		$pos+= $n;
	}
	$_[1]= $pos << 3;
	return $v;
}

sub read_symbol_le {
	#my ($buffer, $bitpos)= @_;
	my $symbol_id= &read_vqty_le;
	my $suffix;
	if ($symbol_id & 1) {
		my $len= &read_vqty_le;
		$len > 0
			or Userp::Error::Domain->throw({ value => $len, min => 1 });
		my $pos= $_[1] >> 3;
		$pos + $len <= length $_[0]
			or die Userp::Error::EOF->for_buf($_[0], $pos, $len, 'symbol suffix');
		$suffix= substr($_[0], $pos, $len);
		$_[1]= ($pos+$len) << 3;
	}
	return ($symbol_id>>1, $suffix);
}

sub read_symbol_be {
	#my ($buffer, $bitpos)= @_;
	my $symbol_id= &read_vqty_be;
	my $suffix;
	if ($symbol_id & 1) {
		my $len= &read_vqty_be;
		$len > 0
			or die Userp::Error::Domain->new({ value => $len, min => 1 });
		my $pos= $_[1] >> 3;
		$pos + $len <= length $_[0]
			or die Userp::Error::EOF->for_buf($_[0], $pos, $len, 'symbol suffix');
		$suffix= substr($_[0], $pos, $len);
		$_[1]= ($pos+$len) << 3;
	}
	return ($symbol_id>>1, $suffix);
}

1;
