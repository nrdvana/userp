package Userp::Bits;

use constant host_is_be => (pack('s', 1) eq "\0\x01");

our @api= qw(
	scalar_bit_size
	pack_bit_size
	twos_minmax
	unsigned_max
	roundup_bits_to_alignment
	sign_extend
	truncate_to_bits
	bitlen
	pack_bits_le
	pack_bits_be
	encode_int_le
	encode_int_be
	decode_int_le
	decode_int_be
	is_valid_symbol
	validate_symbol
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

sub _deep_cmp {
	defined $_[0] cmp defined $_[1]
	or ref $_[0] cmp ref $_[1]
	or (!ref $_[0]? $_[0] cmp $_[1]
		: ref $_[0] eq 'ARRAY'? (
			$#{$_[0]} cmp $#{$_[1]}
			or do { my $x; ($x= _deep_cmp($_[0][$_], $_[1][$_])) && return $x for 0..$#{$_[0]}; 0 }
		)
		: ref $_[0] eq 'HASH'? (
			scalar keys %{$_[0]} cmp scalar keys %{$_[1]}
			or do { my $x; ($x= !exists $_[1]{$_} || _deep_cmp($_[0]{$_}, $_[1]{$_})) && return $x for sort keys %{$_[0]}; 0 }
		)
		: $_[0] <=> $_[1]
	);
}

1;
