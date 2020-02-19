package Userp::Buffer;
use strict;
use warnings;
use Carp;
use Userp::Bits;

sub new {
	my ($class, $bufref, $alignment)= @_;
	bless [ $bufref || \(my $x= ''), $alignment || 0, 0, 0 ], $class;
}

sub buf :lvalue { $_[0][0] }
sub alignment   { $_[0][1] }
sub _bitpos     { $_[0][2] }
sub _readpos    { $_[0][3] }
sub bigendian   { 0 }

*Userp::Buffer::encode_int = *Userp::Bits::buffer_encode_bits_le;
*Userp::Buffer::decode_int = *Userp::Bits::buffer_decode_bits_le;
*Userp::Buffer::encode_vqty= *Userp::Bits::buffer_encode_vqty_le;
*Userp::Buffer::decode_vqty= *Userp::Bits::buffer_decode_vqty_le;

sub seek_to_alignment {
	#($self, $pow2_bytes)
	if ($_[1] > 0) {
		# If asking for multi-byte alignment, the ability to provide this depends on
		# the alignment of the start of the buffer.
		croak "Can't align to 2^$_[1] on a buffer whose starting alignment is 2^$_[0][1]"
			unless $_[0][1] >= $_[1];
		++$_[0][3] if $_[0][2]; # round up to next byte
		my $mask= (1 << $_[1]) - 1;
		my $newpos= ($_[0][3] + $mask) & ~$mask;
		# and make sure that is still within the buffer
		croak "Seek beyond end of buffer"
			unless $newpos <= length(${$_[0][0]});
		$_[0][3]= $newpos;
		$_[0][2]= 0; # no partial byte
	}
	# If asking for byte alignment or less, only matters if bitpos is not 0
	elsif ($_[0][2]) {
		my $mask= (1 << ($_[1]+3)) - 1;
		if ($_[0][2] & $mask) {
			# If it rounded up to the next byte, advance 'pos'
			if (! ($_[0][2]= ($_[0][2] + $mask) & ~$mask) ) {
				++$_[0][3];
			}
		}
	}
	return $_[0];
}

sub pad_to_alignment {
	#($self, $pow2_bytes)
	if ($_[1] > 0) {
		# If asking for multi-byte alignment, the ability to provide this depends on
		# the alignment of the start of the buffer.
		croak "Can't align to 2^$_[1] on a buffer whose starting alignment is 2^$_[0][1]"
			unless $_[0][1] >= $_[1];
		my $mask= (1 << $_[1]) - 1;
		my $len= length ${$_[0][0]};
		my $newlen= ($len + $mask) & ~$mask;
		${$_[0][0]} .= "\0" x ($newlen - $len) if $newlen > $len;
		$_[0][2]= 0; # bitpos is zero
	}
	# If asking for byte alignment or less, only matters if bitpos is not 0
	elsif ($_[0][2]) {
		my $mask= (1 << ($_[1]+3)) - 1;
		$_[0][2]= ($_[0][2] + $mask) & ~$mask & 7; # entire op is modulo 8
	}
	return $_[0];
}

sub encode_bytes {
	#($self, $bytes)
	${$_[0][0]} .= $_[1];
	$_[0][2]= 0; # partial byte is already padded with 0 bits
	$_[0];
}

sub decode_bytes {
	#($self, $count)
	# Round up to the next whole byte
	if ($_[0][2]) {
		$_[0][2]= 0;
		++$_[0][3];
	}
	my $start= $_[0][3];
	# Advance the new pos to the end of these bytes 
	$_[0][3] += $_[1];
	return substr(${$_[0][0]}, $start, $_[1]);
}

sub append_buffer {
	# The starting alignment of the next buffer cannot be greater than the
	# starting alignment of this buffer.
	croak "Can't append buffer aligned to 2^$_[1][1] to buffer whose starting alignment is 2^$_[0][1]"
		unless $_[1][1] <= $_[0][1];
	$_[0]->pad_to_alignment($_[1][1]);
	# Unpleasantness if next buffer is bit-aligned and current buffer has leftover bits
	if ($_[1][1] < 0 && $_[0][2]) {
		croak "Concatenation of bit-aligned buffer is not implemented";
	}
	else {
		${$_[0][0]} .= ${$_[1][0]};
		$_[0][2]= $_[1][2]; # copy leftover bit count
	}
}

sub Userp::Buffer::BE::bigendian { 1 }
@Userp::Buffer::BE::ISA= ( __PACKAGE__ );
*Userp::Buffer::BE::encode_int = *Userp::Bits::buffer_encode_bits_be;
*Userp::Buffer::BE::decode_int = *Userp::Bits::buffer_decode_bits_be;
*Userp::Buffer::BE::encode_vqty= *Userp::Bits::buffer_encode_vqty_be;
*Userp::Buffer::BE::decode_vqty= *Userp::Bits::buffer_decode_vqty_be;

1;
