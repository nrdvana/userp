package Userp::Bits;

our @api= qw(
	scalar_bit_size
	pack_bit_size
	twos_minmax
	unsigned_max
	sign_extend
	pack_bits_le
	pack_bits_be
	pad_buffer_to_alignment
	seek_buffer_to_alignment
	concat_bits_le
	concat_bits_be
	concat_vqty_le
	concat_vqty_be
	read_bits_le
	read_bits_be
	read_vqty_le
	read_vqty_be
	read_symbol_le
	read_symbol_be
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

1;
