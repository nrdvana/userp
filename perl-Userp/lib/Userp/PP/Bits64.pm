package Userp::PP::Bits64;
use Config;
use strict;
use warnings;
no warnings 'portable';
use Math::BigInt;
use Carp;
use Userp::Error;
BEGIN { eval "pack('Q<',1)" or die "pack('Q') not supported on this perl.  Use Bits32.pm instead." }

use constant {
	scalar_bit_size => $Config{uvsize},
	pack_bit_size => 64,
	BUF_ALIGN_ATTR => 1,
	BUF_BITPOS_ATTR => 1,
	BUF_BITOFS_ATTR => 2,
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

sub encode_int_le {
	my ($buf, $value, $bits)= @_;
	if (!defined $bits) {
		$buf->[2]= 0; # automatic alignment to byte
		${$buf->[0]} .= $value < 0x80? pack('C',$value<<1)
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
	}
	# If no bit remainder and bit_count is a whole number of bytes,
	# use the simpler byte-based code:
	elsif (!$buf->[2] && !($bits & 7)) {
		my $n= $bits >> 3;
		${$buf->[0]} .= $n == 1? pack 'C', $value
			: $n == 2? pack 'S<', $value
			: $n == 4? pack 'L<', $value
			: $n == 8? pack 'Q<', $value
			: $n < 8? substr(pack('Q<', $value), 0, $n)
			: scalar reverse substr(Math::BigInt->new($value)->as_bytes, -$n);
	}
	else {
		if (my $remainder_bits= $buf->[2]) {
			# Move the remainder bits from the end of the buffer into the $value
			$bits+= $remainder_bits;
			$value= Math::BigInt->new($value) if $bits > 64;
			$value <<= $remainder_bits;
			$value |= ord substr(${$buf->[0]},-1);
			substr(${$buf->[0]}, -1)= ''; # remove partial byte from end
		}
		my $enc= ref($value)? reverse($value->as_bytes) : pack('Q<',$value);
		my $byte_n= ($bits+7)>>3;
		$enc.= ("\0"x$byte_n) if length($enc) < $byte_n;
		${$buf->[0]} .= substr($enc, 0, $byte_n);
		$buf->[2]= $bits & 7; # calculate new remainder
	}
	$buf;
}

sub encode_int_be {
	my ($buf, $value, $bits)= @_;
	if (!defined $bits) {
		$buf->[2]= 0; # automatic alignment to byte
		${$buf->[0]} .= $value < 0x80? pack('C',$value)
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
	}
	# If no bit remainder and bit_count is a whole number of bytes,
	# use the simpler byte-based code:
	elsif (!$buf->[2] && !($bits & 7)) {
		my $n= $bits >> 3;
		${$buf->[0]} .= $n == 1? pack 'C', $value
			: $n == 2? pack 'S>', $value
			: $n == 4? pack 'L>', $value
			: $n == 8? pack 'Q>', $value
			: $n < 8? substr(pack('Q>', $value), -$n)
			: substr(Math::BigInt->new($value)->as_bytes, -$n);
	}
	else {
		# If remainder bits in buffer, merge final byte of buffer into $value
		if (my $remainder_bits= $buf->[2]) {
			my $prev= ord substr(${$buf->[0]}, -1);
			$prev= Math::BigInt->new($prev) if $bits+$remainder_bits > 64;
			$prev <<= $bits - (8-$remainder_bits);
			$prev |= $value;
			$value= $prev;
			substr(${$buf->[0]}, -1)= ''; # remove partial byte form end
			$bits += $remainder_bits;
		}
		if ($bits & 7) {
			$value= Math::BigInt->new($value) if $bits > 64 && !ref $value;
			$value <<= 8 - ($bits & 7);
		}
		my $enc= ref($value)? $value->as_bytes : pack('Q>',$value);
		my $byte_n= ($bits+7)>>3;
		$enc= ("\0"x$byte_n).$enc if length($enc) < $byte_n;
		${$buf->[0]} .= substr($enc, -$byte_n);
		$buf->[2]= $bits & 7;
	}
	$buf;
}

sub decode_int_le {
	my ($buf, $bits)= @_;
	if (!defined $bits) {
		my $pos= $buf->[3];
		++$pos if $buf->[2];
		$pos < length ${$buf->[0]}
			or die Userp::Error::EOF->for_buf($buf, $pos, 1, 'vqty');
		my $switch= ord substr(${$buf->[0]}, $pos, 1);
		my $v;
		if (!($switch & 1)) {
			++$pos;
			$v= $switch >> 1;
		}
		elsif (!($switch & 2)) {
			$pos+2 <= length ${$buf->[0]}
				or die Userp::Error::EOF->for_buf($buf, $pos, 2, 'vqty-2');
			$v= unpack('S<', substr(${$buf->[0]}, $pos, 2)) >> 2;
			$pos+= 2;
		}
		elsif (!($switch & 4)) {
			$pos+4 <= length ${$buf->[0]}
				or die Userp::Error::EOF->for_buf($buf, $pos, 4, 'vqty-4');
			$v= unpack('L<', substr(${$buf->[0]}, $pos, 4)) >> 3;
			$pos+= 4;
		}
		elsif (!($switch & 8)) {
			$pos+8 <= length ${$buf->[0]}
				or die Userp::Error::EOF->for_buf($buf, $pos, 8, 'vqty-8');
			$v= unpack('Q<', substr(${$buf->[0]}, $pos, 8)) >> 4;
			$pos+= 8;
		} else {
			$pos+6 <= length ${$buf->[0]}
				or die Userp::Error::EOF->for_buf($buf, $pos, 6, 'vqty-N');
			my $n= unpack('S<', substr(${$buf->[0]}, $pos, 2));
			$pos+= 2;
			if ($n == 0xFFFF) {
				$n= unpack('L<', substr(${$buf->[0]}, $pos, 4));
				$pos+= 4;
				$n < 0xFFFF_FFFF or Userp::Error::ImplLimit->throw(message => "Refusing to decode ludicrously large integer value");
			} else {
				$n >>= 4;
			}
			# number of bytes is 8 + $n * 4
			$n= ($n << 2) + 8;
			$pos + $n <= length ${$buf->[0]}
				or die Userp::Error::EOF->for_buf($buf, $pos, $n, 'vqty-'.$n);
			$v= Math::BigInt->from_bytes(scalar reverse substr(${$buf->[0]}, $pos, $n));
			$pos+= $n;
		}
		$buf->[3]= $pos;
		return $v;
	}
	# If bit_pos is a byte boundary and bit_count is a whole number of bytes,
	# use the simpler byte-based code:
	elsif (!$buf->[2] && !($bits & 7)) {
		# Ensure we have this many bits available
		my $n= $bits >> 3;
		$buf->[3] + $n <= length ${$buf->[0]}
			or die Userp::Error::EOF->for_buf($buf, $buf->[3], $n, 'int');
		my $bytes= substr(${$buf->[0]}, $buf->[3], $n);
		$buf->[3] += $n;
		return unpack('Q<', $bytes."\0\0\0\0\0\0\0\0") if length $bytes <= 8;
		return Math::BigInt->from_bytes(scalar reverse $bytes);
	}
	else {
		my $bit_remainder= $buf->[2];
		# Ensure we have this many bits available
		$buf->[3] + (($bit_remainder + $bits + 7)>>3) <= length ${$buf->[0]}
			or die Userp::Error::EOF->for_buf_bits($buf, ($buf->[3]<<3)+$buf->[2], $bits, 'int');
		my $bit_read= $bit_remainder + $bits; # total bits being read including remainder
		my $bytes= substr(${$buf->[0]}, $buf->[3], ($bit_read+7)>>3);
		$buf->[3] += ($bit_read >> 3); # new pos
		$buf->[2]= $bit_read & 7; # new remainder
		# clear highest bits of final byte, if needed
		substr($bytes,-1)= chr( (0xFF >> (8-($bit_read&7))) & ord substr($bytes,-1) )
			if $bit_read & 7;
		if ($bit_read <= 64) {
			my $v= unpack('Q<', $bytes."\0\0\0\0\0\0\0\0");
			return $v >>= $bit_remainder;
		}
		else {
			my $v= Math::BigInt->from_bytes(scalar reverse $bytes);
			$v->brsft($bit_remainder) if $bit_remainder;
			return $v;
		}
	}
}

sub decode_int_be {
	my ($buf, $bits)= @_;
	if (!defined $bits) {
		my $pos= $buf->[3];
		++$pos if $buf->[2];
		$pos < length ${$buf->[0]}
			or die Userp::Error::EOF->for_buf($buf, $pos, 1, 'vqty');
		my $switch= ord substr(${$buf->[0]}, $pos, 1);
		my $v;
		if ($switch < 0x80) {
			++$pos;
			$v= $switch;
		}
		elsif ($switch < 0xC0) {
			$pos+2 <= length ${$buf->[0]}
				or die Userp::Error::EOF->for_buf($buf, $pos, 2, 'vqty-2');
			$v= unpack('S>', substr(${$buf->[0]}, $pos, 2)) & 0x3FFF;
			$pos+= 2;
		}
		elsif ($switch < 0xE0) {
			$pos+4 <= length ${$buf->[0]}
				or die Userp::Error::EOF->for_buf($buf, $pos, 4, 'vqty-4');
			$v= unpack('L>', substr(${$buf->[0]}, $pos, 4)) & 0x1FFF_FFFF;
			$pos+= 4;
		}
		elsif ($switch < 0xF0) {
			$pos+8 <= length ${$buf->[0]}
				or die Userp::Error::EOF->for_buf($buf, $pos, 8, 'vqty-8');
			$v= unpack('Q>', substr(${$buf->[0]}, $pos, 8)) & 0x0FFF_FFFF_FFFF_FFFF;
			$pos+= 8;
		}
		else {
			$pos+6 <= length ${$buf->[0]}
				or die Userp::Error::EOF->for_buf($buf, $pos, 6, 'vqty-N');
			my $n= unpack('S>', substr(${$buf->[0]}, $pos, 2));
			$pos+= 2;
			if ($n == 0xFFFF) {
				$n= unpack('L>', substr(${$buf->[0]}, $pos, 4));
				$pos+= 4;
				$n < 0xFFFF_FFFF or Userp::Error::ImplLimit->throw(message => "Refusing to decode ludicrously large integer value");
			} else {
				$n &= 0xFFF;
			}
			# number of bytes is 8 + $n * 4
			$n= ($n << 2) + 8;
			$pos + $n <= length ${$buf->[0]}
				or die Userp::Error::EOF->for_buf($buf, $pos, $n, 'vqty-'.$n);
			$v= Math::BigInt->from_bytes(substr(${$buf->[0]}, $pos, $n));
			$pos+= $n;
		}
		$buf->[3]= $pos;
		return $v;
	}
	# If bit_pos is a byte boundary and bit_count is a whole number of bytes,
	# use the simpler byte-based code:
	elsif (!$buf->[2] && !($bits & 7)) {
		# Ensure we have this many bits available
		my $n= $bits >> 3;
		$buf->[3] + $n <= length ${$buf->[0]}
			or die Userp::Error::EOF->for_buf_bits($buf, $buf->[3], $n, 'int');
		my $bytes= substr(${$buf->[0]}, $buf->[3], $n);
		$buf->[3] += $n;
		return unpack 'C',  $bytes if $bits == 8;
		return unpack 'S>', $bytes if $bits == 16;
		return unpack 'L>', $bytes if $bits == 32;
		return unpack 'Q>', $bytes if $bits == 64;
		return unpack('Q>', ("\0" x (8-length $bytes)) . $bytes) & ((1 << $bits) - 1)
			if $bits < 64;
		return Math::BigInt->from_bytes($bytes);
	}
	else {
		my $bit_remainder= $buf->[2];
		my $bit_read= $bit_remainder + $bits;
		# Ensure we have this many bits available
		$buf->[3] + (($bit_read + 7)>>3) <= length ${$buf->[0]}
			or die Userp::Error::EOF->for_buf_bits($buf, ($buf->[3]<<3)+$buf->[2], $bits, 'int');
		my $bytes= substr(${$buf->[0]}, $buf->[3], ($bit_read+7)>>3);
		$buf->[3] += ($bit_read >> 3); # new pos
		$buf->[2]= $bit_read & 7; # new remainder
		# clear highest bits of first byte, if needed
		substr($bytes,0,1)= chr( (0xFF >> $bit_remainder) & ord substr($bytes,0,1) )
			if $bit_remainder;
		if ($bit_read <= 64) {
			return unpack('Q>', $bytes."\0\0\0\0\0\0\0\0") >> (64 - $bit_read);
		}
		else {
			my $v= Math::BigInt->from_bytes($bytes);
			$v->brsft(8 - ($bit_read & 7)) if $bit_read & 7;
			return $v;
		}
	}
}

1;
