package Userp::Buffer;
use strict;
use warnings;
use Carp;
use Userp::Bits;

@Userp::Buffer::BE::ISA= ( __PACKAGE__ );

sub new {
	my $class= shift;
	my $self= [ undef, 0, 0, 0 ];
	if (@_) {
		my $opts= @_ == 1 && ref $_[0] eq 'HASH'? $_[0] : { @_ };
		$self->[0]= $opts->{bufref};
		$self->[1]= $opts->{alignment};
		$class .= '::BE' if $opts->{bigendian};
	}
	$self->[0] ||= \(my $X= '');
	bless $self, $class;
}

sub new_le {
	my $class= shift;
	bless [ $_[0] || \(my $x=''), $_[1] || 0, 0, 0 ], $class;
}

sub new_be {
	my $class= shift;
	$class .= '::BE';
	bless [ $_[0] || \(my $x=''), $_[1] || 0, 0, 0 ], $class;
}

sub bufref :lvalue { $_[0][0] }
sub alignment      { $_[0][1] }
sub _bitpos        { $_[0][2] }
sub _readpos       { $_[0][3] }
sub bigendian      { 0 }
sub length         { length ${$_[0][0]} }
sub Userp::Buffer::BE::bigendian { 1 }

*Userp::Buffer::encode_bits= *Userp::Bits::buffer_encode_bits_le;
*Userp::Buffer::decode_bits= *Userp::Bits::buffer_decode_bits_le;
*Userp::Buffer::encode_vqty= *Userp::Bits::buffer_encode_vqty_le;
*Userp::Buffer::decode_vqty= *Userp::Bits::buffer_decode_vqty_le;

*Userp::Buffer::BE::encode_bits= *Userp::Bits::buffer_encode_bits_be;
*Userp::Buffer::BE::decode_bits= *Userp::Bits::buffer_decode_bits_be;
*Userp::Buffer::BE::encode_vqty= *Userp::Bits::buffer_encode_vqty_be;
*Userp::Buffer::BE::decode_vqty= *Userp::Bits::buffer_decode_vqty_be;

sub seek_to_alignment {
	my ($self, $pow2)= @_;
	if ($pow2 > 0) {
		# If asking for multi-byte alignment, the ability to provide this depends on
		# the alignment of the start of the buffer.
		croak "Can't align to 2^$pow2 on a buffer whose starting alignment is 2^$self->[1]"
			unless $self->[1] >= $pow2;
		++$self->[3] if $self->[2]; # round up to next byte
		my $mask= (1 << $pow2) - 1;
		my $newpos= ($self->[3] + $mask) & ~$mask;
		# and make sure that is still within the buffer
		croak "Seek beyond end of buffer"
			unless $newpos <= CORE::length(${$self->[0]});
		$self->[3]= $newpos;
		$self->[2]= 0; # no partial byte
	}
	# If asking for byte alignment or less, only matters if bitpos is not 0
	elsif ($self->[2]) {
		my $mask= (1 << ($pow2+3)) - 1;
		if ($self->[2] & $mask) {
			# If it rounded up to the next byte, advance 'pos'
			if (! ($self->[2]= ($self->[2] + $mask) & ~$mask) ) {
				++$self->[3];
			}
		}
	}
	return $self;
}

sub pad_to_alignment {
	my ($self, $pow2)= @_;
	if ($pow2 > 0) {
		# If asking for multi-byte alignment, the ability to provide this depends on
		# the alignment of the start of the buffer.
		croak "Can't align to 2^$pow2 on a buffer whose starting alignment is 2^$self->[1]"
			unless $self->[1] >= $pow2;
		my $mask= (1 << $pow2) - 1;
		my $len= CORE::length ${$self->[0]};
		my $newlen= ($len + $mask) & ~$mask;
		${$self->[0]} .= "\0" x ($newlen - $len) if $newlen > $len;
		$self->[2]= 0; # bitpos is zero
	}
	# If asking for byte alignment or less, only matters if bitpos is not 0
	elsif ($self->[2]) {
		my $mask= (1 << ($pow2+3)) - 1;
		$self->[2]= ($self->[2] + $mask) & ~$mask & 7; # entire op is modulo 8
	}
	return $self;
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
	my ($self, $peer)= @_;
	# The starting alignment of the next buffer cannot be greater than the
	# starting alignment of this buffer.
	croak "Can't append buffer aligned to 2^$peer->[1] to buffer whose starting alignment is 2^$self->[1]"
		unless $peer->[1] <= $self->[1];
	$self->pad_to_alignment($peer->[1]);
	# Unpleasantness if next buffer is bit-aligned and current buffer has leftover bits
	if ($peer->[1] < 0 && $self->[2]) {
		croak "Concatenation of bit-aligned buffer is not implemented";
	}
	else {
		${$self->[0]} .= ${$peer->[0]};
		$self->[2]= $peer->[2]; # copy leftover bit count
	}
}

1;
