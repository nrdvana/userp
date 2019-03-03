package Userp::PP::Decoder;
use Moo;
use Carp;

has scope      => ( is => 'ro', required => 1 );
has buffer     => ( is => 'ro', required => 1 );
has bigendian  => ( is => 'rw', trigger => \&_change_endian );
has _remainder => ( is => 'rw' );
has _iterstack => ( is => 'rw' );

push @Userp::PP::Decoder::BE::ISA, __PACKAGE__;

sub BUILD {
	my ($self, $args)= @_;
	$self->_change_endian;
	pos(${$self->buffer}) ||= 0;
}

sub _change_endian {
	my $self= shift;
	my $be= $self->bigendian;
	if ($be && ref($self) !~ /::BE$/) { bless $self, ref($self).'::BE'; }
	if (!$be && ref($self) =~ /(.*?)::BE$/) { bless $self, $1; }
}

our $EOF= \"EOF";

sub node_type {
	my $self= shift;
	return $self->parent->buffer_type unless $self->_iterstack;
	return $self->_iterstack->[-1]{type};
}

sub decode {
	my ($self)= @_;
	my $type= $self->_decode_typeid;
	my $value= $self->_decode($type);
	return $value;
}

sub decode_intX {
	my ($self, $signed, $recursive)= @_;
	pos ${$self->buffer} < length ${$self->buffer} or die $EOF;
	my $switch= unpack('C', @{$self->buffer});
	if (!($switch & 1)) {
		++ pos ${$self->buffer};
		return ($signed && ($switch&0x80)? $switch - 0x100 : $switch) >> 1;
	}
	return !($switch & 2)? ($signed? do { use integer; $self->decode_intN(16, 1) >> 2 } : $self->decode_intN(16) >> 2)
		: !($switch & 4)?  ($signed? do { use integer; $self->decode_intN(32, 1) >> 3 } : $self->decode_intN(32) >> 3)
		: !($switch & 8)?  ($signed? do { use integer; $self->decode_intN(48, 1) >> 4 } : $self->decode_intN(48) >> 4)
		: $switch != 0xFF? ($signed? do { use integer; $self->decode_intN(64, 1) >> 4 } : $self->decode_intN(64) >> 4)
		: do {
			croak "Refusing to decode ludicrously large integer value" if $recursive;
			$self->decode_intN(8 * (8 + $self->decode_intX(0,1)), $signed);
		};
}

sub Userp::PP::Decoder::BE::decode_intX {
	my ($self, $signed, $recursive)= @_;
	pos ${$self->buffer} < length ${$self->buffer} or die $EOF;
	my $switch= unpack('C', @{$self->buffer});
	if ($switch < 0x80) {
		++ pos ${$self->buffer};
		return $signed && ($switch & 0x40)? $switch - 0x80 : $switch;
	}
	return $self->decode_intN(
		$switch < 0xC0? 14
		: $switch < 0xE0? 29
		: $switch < 0xF0? 44
		: $switch < 0xFF? 60
		: do {
			croak "Refusing to decode ludicrously large integer value" if $recursive;
			8 * (8 + $self->decode_intX(0,1));
		},
		$signed
	);
}

sub decode_intN {
	my ($self, $bits, $signed)= @_;
	my $bytes= ($bits+7) >> 3;
	(pos(${$self->buffer}) + $bytes) <= length ${$self->buffer} or die $EOF;
	my $buf= substr ${$self->buffer}, pos(${$self->buffer}), $bytes;
	pos(${$self->buffer}) += $bytes;
	return $signed? _sign_extend($self->_int_from_bytes($buf), $bits) : $self->_int_from_bytes($buf);
}

my $have_pack_Q= eval { pack "Q>", 1 }? 1 : 0;

sub _int_from_bytes {
	my ($self, $bytes)= @_;
	length $bytes <= 4? (
		length $bytes == 1? unpack('C', $bytes)
		: length $bytes == 2? unpack('v', $bytes)
		: length $bytes == 4? unpack('V', $bytes)
		:                     unpack('V', $bytes."\0")
	) : length $bytes <= 8 && $have_pack_Q? (
		length $bytes == 8? unpack('Q<', $bytes)
		: unpack('Q<', $bytes . "\0\0\0")
	) : Math::BigInt->from_bytes(reverse $bytes);
}

sub Userp::PP::Decoder::BE::_int_from_bytes {
	my ($self, $bytes)= @_;
	length $bytes <= 4? (
		length $bytes == 1? unpack('C', $bytes)
		: length $bytes == 2? unpack('n', $bytes)
		: length $bytes == 4? unpack('N', $bytes)
		:                     unpack('N', "\0".$bytes)
	) : length $bytes <= 8 && $have_pack_Q? (
		length $bytes == 8? unpack('Q>', $bytes)
		: unpack('Q>', ("\0"x(8-length $bytes)).$bytes)
	) : Math::BigInt->from_bytes($bytes);
}

sub _sign_extend {
	my ($val, $bits)= @_;
	# Mask out other bits, if needed
	if ($bits > ($have_pack_Q? 64 : 32)) {
		use bigint;
		$val &= (1 << $bits)-1;
		$val -= (1 << $bits) if $val & (1 << ($bits-1));
	}
	else {
		$val &= (1 << $bits)-1;
		$val -= (1 << $bits) if $val & (1 << ($bits-1));
	}
	$val;
}

1;
