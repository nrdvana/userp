package Userp::Decoder;
use Moo;
use Carp;
use Userp::Bits;
use Userp::Error;

=head1 SYNOPSIS

  my $decoder= Userp::Decoder->new(scope => $s, buffer => $b, bigendian => 0);
  
  # one-shot recursive decode
  my $perl_data= $dec->decode;
  
  # or iterate the data as you decode it
  if ($dec->node_is_array) {
    my @array;
    $dec->begin;
    push @array, $dec->decode
      while $dec->node_is_defined;
    $dec->end;
  }
  
  # introspect the data being decoded
  $dec->field_name;
  $dec->array_index;
  $dec->node_type;
  $dec->value_type;
  $dec->as_string;
  $dec->as_number;

=head1 DESCRIPTION

This class is the API for deserializing data from known Userp types into Perl data.
A new decoder is created for each block of a Stream, and given a Scope and Endian flag
and initial Block Root Type.

The decoder tracks its position within the buffer, and a "current node" which refers to the
type that should be encoded at this position.  A block is a single node, but if it is an array
or record you can "begin" the node to inspect its component nodes, calling "next" or "seek"
to traverse within the parent node.

The API for each general Userp type are as follows:

=over

=item Integer

  $dec->value_is_int;            # will be true
  $int_val= $dec->value_number;  # read value without advancing
  $int_val= $dec->decode_number; # read value and advance to next node
  $int_val= $dec->decode;        # same as decode_number
  
  # for Integer with a named value
  $dec->value_is_sym;            # will be true
  $sym_name= $dec->value_symbol; # get symbolic name of value without advancing
  $int= $dec->decode;            # default is still to decode as a number

=item Symbol

  $dec->value_is_sym;            # will be true
  $sym= $dec->value_symbol;      # read value without advancing
  $sym= $dec->decode_symbol;     # read value and advance to next node

=item Choice

  $dec->node_type->is_choice;    # will be true
  $dec->selector;                # arrayref of choice type selections
  
  # the value is never a choice, so inspect $dec->value_type and $dec->value_is_...
  # in order to determine what methods to use to extract the value.

=item Array

  $dec->value_is_array;          # will be true
  $dec->value_is_string;         # can be true
  $dec->array_dim;               # arrayref of dimensions of array
  $dec->array_elem_type;         # type of each element of array (May be 'Any')
  $dec->value_array;             # returns arrayref with each delement decoded
  $dec->begin;                   # enter array node and iterate element nodes
  $i= $dec->node_index;          # After entering array, returns index of node
  $dec->seek( $idx );            # After entering array, seek to array element
  $dec->end;                     # After entering array, seek to node following the array
  
  # if Array is string-compatible:
  $string= $dec->value_string;
  $string= $dec->decode_string;

=item Record

  $dec->value_is_record;         # will be true
  $dec->begin;                   # enter record node and iterate field nodes
  $n= $dec->node_name;           # After entering record, returns Symbol of field name
  $dec->seek( $node_name );      # Seek to named field
  $dec->end;                     # After entering record, seek to node following record

=back

=cut

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
	if (defined (my $bits= $type->bitsizeof)) {
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
	my $bits= $type->bitsizeof;
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
