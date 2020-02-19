package Userp::Bits;

use constant host_is_be => (pack('s', 1) eq "\0\x01");

our @api= qw(
	scalar_bit_size
	pack_bit_size
	twos_minmax
	unsigned_max
	sign_extend
	bitlen
	pack_bits_le
	pack_bits_be
	buffer_encode_bits_le
	buffer_encode_bits_be
	buffer_encode_vqty_le
	buffer_encode_vqty_be
	buffer_decode_bits_le
	buffer_decode_bits_be
	buffer_decode_vqty_le
	buffer_decode_vqty_be
);

our $mechanism; # can be set before loading package to request specific source
$mechanism ||= eval "require 'Userp::XS::Bits'"? 'xs'
	: eval "pack('Q<',1)"? '64'
	: '32';

if ($mechanism eq 'xs') {
	require Userp::XS::Bits;
	*$_ = *{"Userp::XS::Bits::$_"} for @api;
}
elsif ($mechanism eq '64') {
	require Userp::PP::Bits64;
	*$_ = *{"Userp::PP::Bits64::$_"} for @api;
}
else {
	require Userp::PP::Bits32;
	*$_ = *{"Userp::PP::Bits32::$_"} for @api;
}

sub is_valid_symbol {
	!!( $_[0] =~ /^[^\0-\x1F]+$/ )
}

1;
