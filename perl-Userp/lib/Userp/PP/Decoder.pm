package Userp::PP::Decoder;
use Moo;
use Carp;
use Math::BigInt;
my $have_pack_Q; BEGIN { $have_pack_Q= eval { pack "Q>", 1 }? 1 : 0; }

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

sub decode_Integer {
	my ($self, $type)= @_;
	if (defined (my $bits= $type->const_bitlen)) {
		return $type->min_val unless $bits;
		return $self->decode_intN($bits) + $type->min_val;
	}
	my $ofs= $self->decode_intX;
	return $type->min_val + $ofs if defined $type->min_val;
	return $type->max_val - $ofs if defined $type->max_val;
	return (($ofs & 1)? (-$ofs-1) : $ofs) >> 1;
}

sub decode_Enum {
	my ($self, $type)= @_;
	if ($type->encode_literal) {
		croak "Unimplemented";
		my $val= $self->decode_Integer($type->members->[0][1]);
	}
	my $bits= $type->const_bitlen;
	my $val= $self->decode_intN($bits);
	croak "Enum out of bounds" if $val > $#{$type->members};
	return $type->members->[$val][0];
}

sub decode_Union {
	croak "Unimplemented";
}
=cut
sub decode_Sequence {
	croak "Unimplemented";
}
=cut
sub decode_intX {
	my $self= shift;
	return delete $self->{remainder} if defined $self->{remainder};
	pos ${$self->buffer} < length ${$self->buffer} or die $EOF;
	my $switch= unpack('C', ${$self->buffer});
	if (!($switch & 1)) {
		++ pos ${$self->buffer};
		return $switch >> 1;
	}
	return !($switch & 2)? $self->decode_intN(16) >> 2
		:  !($switch & 4)? $self->decode_intN(32) >> 3
		: $switch != 0xFF? $self->decode_intN(64) >> 3
		: do {
			++ pos ${$self->buffer};
			my $n= $self->decode_intN(8);
			$n= $self->decode_intN(16) if $n == 0xFF;
			croak "Refusing to decode ludicrously large integer value" if $n == 0xFFFF;
			$self->decode_uintN(8 * (8 + $n));
		};
}

sub Userp::PP::Decoder::BE::decode_intX {
	my $self= shift;
	return delete $self->{remainder} if defined $self->{remainder};
	pos ${$self->buffer} < length ${$self->buffer} or die $EOF;
	my $switch= unpack('C', ${$self->buffer});
	if ($switch < 0x80) {
		++ pos ${$self->buffer};
		return $switch;
	}
	return $self->decode_intN(
		$switch < 0xC0? 14
		: $switch < 0xE0? 29
		: $switch < 0xFF? 61
		: do {
			++ pos ${$self->buffer};
			my $n= $self->decode_intN(8);
			$n= $self->decode_intN(16) if $n == 0xFF;
			croak "Refusing to decode ludicrously large integer value" if $n == 0xFFFF;
			8 * (8 + $n)
		}
	);
}

sub decode_intN {
	my ($self, $bits)= @_;
	my $bytes= ($bits+7) >> 3;
	(pos(${$self->buffer}) + $bytes) <= length ${$self->buffer} or die $EOF;
	my $buf= substr ${$self->buffer}, pos(${$self->buffer}), $bytes;
	pos(${$self->buffer}) += $bytes;
	my $val= $self->_int_from_bytes($buf);
	if ($bits & 7) {
		# If not a whole number of bytes, clear some bits at the top
		if ($bits > ($have_pack_Q? 64 : 32)) {
			use bigint;
			$val &= (1 << $bits)-1;
		}
		else {
 			$val &= (1 << $bits)-1;
		}
	}
	return $val;
}

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
	) : Math::BigInt->from_bytes(scalar reverse $bytes);
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

1;
