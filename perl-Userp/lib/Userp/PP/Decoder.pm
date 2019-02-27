package Userp::PP::Decoder;
use Moo;

has scope      => ( is => 'ro', required => 1 );
has block      => ( is => 'ro', required => 1 );
has _remainder => ( is => 'rw' );
has _iterstack => ( is => 'rw' );

our $EOF= \"EOF";

sub node_type {
	my $self= shift;
	return $self->parent->block_type unless $self->_iterstack;
	return $self->_iterstack->[-1]{type};
}

sub decode {
	my ($self)= @_;
	my $type= $self->_decode_typeid;
	my $value= $self->_decode($type);
	return $value;
}

sub decode_uintX {
	my $self= shift;
	if ($self->bigendian) {
		${$self->block} =~ /\G(
				[\0-\x7F] | [\x80-\xBF]. | [\xC0-\xDF].{3} | [\xE0-\xEF].{5}
				| [\xF0-\xFE].{7} | \xFF (.)(.{8})
			)/xgc or die $EOF;
		return ord $1 if length $1 == 1;
		return 0x3FFF & unpack('n', $1) if length $1 == 2;
		return 0x1FFFFFFF & unpack('N', $1) if length $1 == 4;
		return 0x0FFF_FFFFFFFF & unpack('Q>', "\0\0".$1) if length $1 == 6;
		return 0x0FFFFFFF_FFFFFFFF & unpack('Q>', $1) if length $1 == 8;
		my $bytes= ord $2;
		$bytes == 0 or croak "BigInteger values not supported yet";
		return unpack('Q>', $3);
	}
	else {
		${$self->block} =~ /\G(
				[\x00\x02\x04\x06\x08\x0A\x0C\x0E\x10\x12\x14\x16\x18\x1A\x1C\x1E]
				| [\x20\x22\x24\x26\x28\x2A\x2C\x2E\x30\x32\x34\x36\x38\x3A\x3C\x3E]
				| [\x40\x42\x44\x46\x48\x4A\x4C\x4E\x50\x52\x54\x56\x58\x5A\x5C\x5E]
				| [\x60\x62\x64\x66\x68\x6A\x6C\x6E\x70\x72\x74\x76\x78\x7A\x7C\x7E]
				| [\x80\x82\x84\x86\x88\x8A\x8C\x8E\x90\x92\x94\x96\x98\x9A\x9C\x9E]
				| [\xA0\xA2\xA4\xA6\xA8\xAA\xAC\xAE\xB0\xB2\xB4\xB6\xB8\xBA\xBC\xBE]
				| [\xC0\xC2\xC4\xC6\xC8\xCA\xCC\xCE\xD0\xD2\xD4\xD6\xD8\xDA\xDC\xDE]
				| [\xE0\xE2\xE4\xE6\xE8\xEA\xEC\xEE\xF0\xF2\xF4\xF6\xF8\xFA\xFC\xFE]
				| [\x01\x05\x09\x0D\x11\x15\x19\x1D\x21\x25\x29\x2D\x31\x35\x39\x3D].
				| [\x41\x45\x49\x4D\x51\x55\x59\x5D\x61\x65\x69\x6D\x71\x75\x79\x7D].
				| [\x81\x85\x89\x8D\x91\x95\x99\x9D\xA1\xA5\xA9\xAD\xB1\xB5\xB9\xBD].
				| [\xC1\xC5\xC9\xCD\xD1\xD5\xD9\xDD\xE1\xE5\xE9\xED\xF1\xF5\xF9\xFD].
				| [\x03\x0B\x13\x1B\x23\x2B\x33\x3B\x43\x4B\x53\x5B\x63\x6B\x73\x7B].{3}
				| [\x83\x8B\x93\x9B\xA3\xAB\xB3\xBB\xC3\xCB\xD3\xDB\xE3\xEB\xF3\xFB].{3}
				| [\x07\x17\x27\x37\x47\x57\x67\x77\x87\x97\xA7\xB7\xC7\xD7\xE7\xF7].{5}
				| [\x0F\x1F\x2F\x3F\x4F\x5F\x6F\x7F\x8F\x9F\xAF\xBF\xCF\xDF\xEF].{7}
				| \xFF (.)(.{8})
			)/xgc or die $EOF;
		return ord($1)>>1 if length $1 == 1;
		return unpack('v', $1)>>2 if length $1 == 2;
		return unpack('V', $1)>>3 if length $1 == 4;
		return unpack('Q<', $1."\0\0")>>4 if length $1 == 6;
		return unpack('Q<', $1)>>4 if length $1 == 8;
		my $bytes= ord $2;
		$bytes == 0 or croak "BigInteger values not supported yet";
		return unpack('Q<', $3);
	}
}

1;
