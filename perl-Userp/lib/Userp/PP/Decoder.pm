package Userp::PP::Decoder;
use Moo;
use Carp;
use Math::BigInt;

=head1 DESCRIPTION

This class is the API for deserializing data from known Userp types into Perl data.

When decoding, there is always a "current node" which has a type.  In the case of a Union
and Enum, the type will have a "outer type" and "inner type", and possibly several types
inbetween, forming a chain.  You can choose to iterate this chain, or ignore it.
In the case of Enums, the value may have an Identifier.  You can read the identifier, or
value, or both.  In the case of Sequences, you can choose to iterate the sequence or skip
over it, or possibly read it into another data structure like a String as a single operation.

The decoding API operates roughly like this:

  $type= $dec->node_type;
  $value= $dec->node_${PERLTYPE}_val;
  $dec->next;

There is also a shortcut for each perl-type called C<decode_${PERLTYPE}> which performs a pair
of C<node_X_val> followed by C<next>.

The patterns for each general Userp type are as follows:

=over

=item Integer

  $dec->node_is_int;             # will be true
  $int_val= $dec->node_int_val;
  $int_val= $dec->decode_int;

=item Enum

  $dec->node_is_enum;            # will be true
  $enum_type= $dec->node_type;   # outer type is enum
  $type= $dec->node_type(1);     # $enum_type->member_type (i.e. thing actually being decoded)
  $val= $dec->node_enum_index;   # index of enum member; always defined
  $ident= $dec->node_ident_val;  # can be NULL if enum doesn't name all values
  $val= $dec->decode_enum_index; # returns the enum member index; always defined
  $val= $dec->decode_ident;      # returns the identifier; can be NULL
  
  # To retrieve literal enum value, use any normal decode methods appropriate to that type
  $val= $dec->decode_int;
  $val= $dec->decode_float;
  $val= $dec->decode_array;
  ... # etc

=item Union

  $union_type= $dec->node_type;      # returns the type of the union itself
  $subtype   = $dec->node_type(1)    # selected type within union
  # and there can be further sub-types, recursively through unions and enums
  $leaf_type = $dec->node_type(-1)   # the leaf-type, i.e. thing actually being decoded
  $union_depth= $dec->node_is_union  # returns the *count* of nested unions

In the case of a union of enums, node_type(-1) refers to the enum's member type, and
node_type(-2) refers to the enum type itself, but C<node_is_enum> will still return true.

=item Sequence

  if ($dec->node_is_seq) {
    $dec->enter_seq;
    while ($dec->node_type) {
      my $ident= $dec->node_elem_ident; # not ident_val, because the value might also be an ident
      # decode the node
      ...
      $dec->next;
    }
  }
  
  # or, if sequence is array-compatible:
  my $arrayref= $dec->decode_array;

  # or, if sequence is record-compatible:
  my $hashref= $dec->decode_record;
  
  # or if sequence is string-compatible:
  my $string= $dec->decode_string;

=item Ident

Identifiers are not expected to be used as values much, but can be.

  $ident= $enc->node_ident_val;
  $ident= $enc->decode_ident;

=back

=cut

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
		return $self->decode_qty($bits) + $type->min_val;
	}
	my $ofs= $self->decode_vqty;
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
	my $val= $self->decode_qty($bits);
	croak "Enum out of bounds" if $val > $#{$type->members};
	return $type->members->[$val][0];
}

sub decode_Union {
	croak "Unimplemented";
}

=cut
sub decode_Sequence {
	my ($self, $type)= @_;
	my $len= $type->len_type? $self->decode_Integer($type->len_type) : $type->const_len;
	my $elem_spec= $type->elem_spec;
	my @result;
	while (1) {
		my ($elem_idx, $elem_type, $elem_ident);
		if ($type->sparse) {
			if ($type->named_elems) {
				# Spare named elems which might refer to elem_spec:
				# Read a signed var-int.  Values 0..#members refer to elem_spec, values above #members
				# are an identifier ID, and negative values are an inline identifier. 
				my $val= $self->decode_vqty;
				if ($val < 0) {
					$elem_ident= $self->decode_ident_suffix;
					$elem_type= $self->decode_typeid;
				} elsif (!$val < 
		my $i= @result;
		my $elem_type= ref $elem_spec ne 'ARRAY'? $elem_spec
			: $i < @{$elem_spec}? ( ref $elem_spec->[$i] eq 'ARRAY'? $elem_spec->[$i][0] : $elem_spec->[$i] )
			: undef;
		my $
	croak "Unimplemented";
}
=cut

sub decode_vqty {
	my $self= shift;
	return delete $self->{remainder} if defined $self->{remainder};
	pos ${$self->buffer} < length ${$self->buffer} or die $EOF;
	my $switch= unpack('C', ${$self->buffer});
	if (!($switch & 1)) {
		++ pos ${$self->buffer};
		return $switch >> 1;
	}
	return !($switch & 2)? $self->decode_qty(16) >> 2
		:  !($switch & 4)? $self->decode_qty(32) >> 3
		: $switch != 0xFF? $self->decode_qty(64) >> 3
		: do {
			++ pos ${$self->buffer};
			my $n= $self->decode_qty(8);
			$n= $self->decode_qty(16) if $n == 0xFF;
			croak "Refusing to decode ludicrously large integer value" if $n == 0xFFFF;
			$self->decode_qty(8 * (8 + $n));
		};
}

sub Userp::PP::Decoder::BE::decode_vqty {
	my $self= shift;
	return delete $self->{remainder} if defined $self->{remainder};
	pos ${$self->buffer} < length ${$self->buffer} or die $EOF;
	my $switch= unpack('C', ${$self->buffer});
	if ($switch < 0x80) {
		++ pos ${$self->buffer};
		return $switch;
	}
	return $self->decode_qty(
		$switch < 0xC0? 14
		: $switch < 0xE0? 29
		: $switch < 0xFF? 61
		: do {
			++ pos ${$self->buffer};
			my $n= $self->decode_qty(8);
			$n= $self->decode_qty(16) if $n == 0xFF;
			croak "Refusing to decode ludicrously large integer value" if $n == 0xFFFF;
			8 * (8 + $n)
		}
	);
}

sub decode_qty {
	my ($self, $bits)= @_;
	my $bytes= ($bits+7) >> 3;
	(pos(${$self->buffer}) + $bytes) <= length ${$self->buffer} or die $EOF;
	my $buf= substr ${$self->buffer}, pos(${$self->buffer}), $bytes;
	pos(${$self->buffer}) += $bytes;
	my $val= $self->_qty_from_bytes($buf);
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

sub _qty_from_bytes {
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

sub Userp::PP::Decoder::BE::_qty_from_bytes {
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
