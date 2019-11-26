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

has scope        => ( is => 'ro', required => 1 );
has bigendian    => ( is => 'ro' );
has buffer_ref   => ( is => 'rwp' );
sub buffer          { ${ shift->buffer_ref } }
has root_type    => ( is => 'ro', required => 1 );

has _bitpos    => ( is => 'rw' );
has _remainder => ( is => 'rw' );
has _iterstack => ( is => 'rw' );

sub node_type  { $_[0]{_iterstack}[-1]{node_type} }
sub node_index { $_[0]{_iterstack}[-1]{i} }
sub node_name  { $_[0]{_iterstack}[-1]{fieldname} }
sub value_type { $_[0]{_iterstack}[-1]{value_type} }

sub BUILD {
	my ($self, $args)= @_;
	# If user supplied 'buffer' instead of buffer_ref, make a ref automatically.
	if (!$self->buffer_ref && defined $args->{buffer}) {
		$self->_set_buffer_ref(\(my $buf= $args->{buffer}));
	}
	pos(${$self->buffer_ref}) ||= 0; # user can set position, but needs to be defined regardless
	$self->_iterstack([{ node_type => $self->root_type }]);
	$self->_parse_node;
}

sub _parse_node {
	my $self= shift;
	my $it= $self->_iterstack->[-1];
	my $t= $it->{node_type};
	if ($t->isa_int) {
		return $self->_parse_int;
	} elsif ($t->isa_array) {
		return $self->_parse_array;
	} else {
		croak "unimplemented";
	}
}

sub _parse_int {
	my ($self)= @_;
	my $it= $self->_iterstack->[-1];
	my $t= $it->{value_type};
	$it->{int}= $self->_decode_int($t);
	if ($t->names) {
		$it->{sym}= $t->_name_by_val->{$it->{int}};
	}
}

sub _parse_array {
	my ($self)= @_;
	my $it= $self->_iterstack->[-1];
	my $t= $it->{value_type};
	$it->{elem_type}= $t->elem_type || $self->_decode_typeref;
	$it->{dims}= $t->dims || [ (undef) x $self->_decode_vqty ];
	my $n= 1;
	for (@{ $it->{dims} }) {
		$_= $t->dim_type? $self->_decode_int($t->dim_type) : $self->_decode_vqty
			unless defined $_;
		$n *= $_;
	}
	$it->{n}= $n;
}

sub _begin_array {
	my ($self, $indexed)= @_;
	my $it= $self->_iterstack->[-1];
	my $t= $it->{value_type};
	push @{ $self->_iterstack }, { node_type => $it->{elem_type}, node_index => 0 };
	if ($it->{elem_pos}) {
		my $p= pos ${ $self->buffer_ref };
		$it->{elem_pos}{0}= $self->_bitpos? [ $self->_bitpos, $p ] : $p;
	}
	$self->_parse_node;
}

sub _decode_int {
	my ($self, $t)= @_;
	if ($t->bitlen) {
		my $v= $self->_decode_qty($t->bitlen);
		if ($t->bits && $t->min < 0) {
			# sign-extend
			$v= Userp::Bits::sign_extend($v, $t->bits);
		}
		Userp::Error::Domain->assert_minmax($v, $t->min, $t->max, 'Value of type '.$t->name);
		return $v;
	} elsif (defined $t->min) {
		return $t->min + $self->_decode_vqty;
	} elsif (defined $t->max) {
		return $t->max - $self->_decode_vqty;
	} else {
		my $qty= $self->_decode_vqty($t->bits);
		return $qty & 1? -($qty>>1) : ($qty>>1);
	}
}

sub _decode_qty {
	my ($self, $bits)= @_;
	$bits > 0 or croak "BUG: bits must be positive in decode_qty($bits)";
	$self->_iterstack->[-1]{align} < 3? (
		$self->bigendian
			? Userp::Bits::read_bits_be(${$self->{buffer_ref}}, $self->{_bitpos}, $bits)
			: Userp::Bits::read_bits_le(${$self->{buffer_ref}}, $self->{_bitpos}, $bits)
	) : (
		$self->bigendian
			? Userp::Bits::read_int_be(${$self->{buffer_ref}}, $bits)
			: Userp::Bits::read_int_le(${$self->{buffer_ref}}, $bits)
	)
}

sub _decode_vqty {
	my ($self, $value)= @_;
	$value >= 0 or croak "BUG: value must be positive in decode_vqty($value)";
	if ($self->{_bitpos}) {
		++pos(${ $self->buffer_ref });
		$self->{_bitpos}= 0;
	}
	$self->bigendian
		? Userp::Bits::read_vqty_be(${$self->{buffer_ref}})
		: Userp::Bits::read_vqty_le(${$self->{buffer_ref}})
}

1;
