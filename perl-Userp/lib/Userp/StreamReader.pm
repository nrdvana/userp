package Userp::StreamReader;
use Moo;

=head1 SYNOPSIS

  # Blocking I/O Style
  my $st= Userp::StreamReader->new(fh => $fh);
  while (my $block= $st->next_block) { # reads from $fh until a whole block
    ...
  }

  # Event-Callback I/O Style
  my $st= Userp::StreamReader->new();
  ...on_read => sub($fh) {   # Event: $fh is readable
    $st->read($fh);          # one sysread, appends to internal buffer
    while (my $block= $st->next_block) {
      ...
    }
  };

  # ZeroCopy I/O Style 
  my $st= Userp::StreamReader->new();
  while (my $block= $st->parse_inplace(\$buffer, $offset)) { # modifies $offset
    ...
    # release reference to $block before changing $buffer in any way
  }

=head1 DESCRIPTION

This object reads the Userp Stream protocol.  Streams begin with a header, followed by metadata
blocks (which define types and string tables) and data blocks.  Each block must be fully read
into memory before it can be decoded.

The reader can operate in L</read>, "autoread", or L</parse_inplace> mode.  C<read> adds data to
an internal buffer, and then you find out if any complete blocks are available by calling
L</next_block>.  If you supply the L</fh> attribute, you can just call C<next_block> and it
will internally C<read> as needed until it either has a block or reaches EOF.
The C<parse_inplace> mode skips the internal buffer entirely to parse a buffer that you manage.
This allows the library to decode blocks without needing to make copies, however you must ensure
that the buffer does not change during the decoding of a block.

=head1 ATTRIBUTES

=over

=item bigendian

Whether the integers in the stream are written most-significant-byte-first.  This is detected
from the header of the stream on the first read, but is C<undef> until then.

=item proto_major

Major protocol version detected in the stream.  (C<undef> until header has been read)

=item proto_minor

Minor protocol version detected in the stream.  (C<undef> until header has been read)

=item writer_sig

Signature of the writer that produced the stream.  Up to 18 bytes.  If it was valid UTF-8, this
will be a decoded string, else just the bytes.

=item fh

If you supply this file handle to the constructor, calls to L</next_block> will block execution
until a full data block is available.  Else you need to use L</read> or L</parse_inplace>.

=item bytes_read

Counter of number of bytes consumed from the stream.

=back

=cut

has fh             => ( is => 'ro' );
has bigendian      => ( is => 'rwp' );
has proto_major    => ( is => 'rwp' );
has proto_minor    => ( is => 'rwp' );
has writer_sig     => ( is => 'rwp' );
has bytes_read     => ( is => 'rwp' );

has _buffer        => ( is => 'rw' );
has _eof           => ( is => 'rw' );
has _want_scopeid  => ( is => 'rw' );
has _want_blockid  => ( is => 'rw' );
has _want_blocklen => ( is => 'rw' );

=head1 METHODS

=head2 new

Standard L<Moo> constructor.

=head2 read

  $got= $stream->read;              # read from $stream->fh
  $got= $stream->read($fh);         # read from $fh
  $got= $stream->read($fh, $count); # read from $fh, attempting $count
  $got= $stream->read($bytes);      # append bytes directly

Call this method to append additional data to the internal buffer.  If not given literal data,
it makes one call to C<sysread>, calling C<croak> on fatal errors or returning the return value
of C<sysread> otherwise.

=cut

sub read {
	my ($self, $source, $count)= @_;
	if (defined $self->fh) {
		defined $source and croak "Can't pass arguments to read() when fh is defined";
		$source= $self->fh;
	}
	if (!defined $source) {
		croak 'Must specify read($source) when fh attribute is not given';
	}
	elsif (!ref $source) {
		my $n= length $self->{_buffer};
		$self->{_buffer} .= defined $count? substr($source, 0, $count) : $source;
		return length $self->{_buffer} - $n;
	}
	elsif (ref $source eq 'SCALAR') {
		my $n= length $self->{_buffer};
		$self->{_buffer} .= defined $count? substr($$source, 0, $count) : $$source;
		return length $self->{_buffer} - $n;
	}
	else {
		my $got= sysread($source, $self->{_buffer}, length($self->{_buffer}), $count || 16*4096);
		return $got if $got;
		if (defined $got0 {
			$self->_eof(1);
			return 0;
		}
		return undef if $!{EAGAIN} || $!{EINTR} || $!{EWOULDBLOCK};
		Userp::Error::System->throw(syscall => "sysread");
	}
}

sub _read_until {
	my ($self, $goal)= @_;
	while (length $self->{_buffer} < $goal) {
		my $got= $self->read;
		if (!defined $got) { # if asked to block on a non-blocking handle, block using select()
			vec(my $rin= '', fileno($self->fh), 1) = 1;
			select($rin, undef, $rin, undef); # sleep until handle is readable
		}
		elsif (!$got) {
			$self->_eof(1);
			return 0;
		}
		# else making progress, so continue loop
	}
	return 1;
}
sub _read_more {
	my $self= shift;
	$self->_read_until(1 + length $self->{_buffer});
}

=head2 parse_inplace

  my $block= $stream->parse_inplace( \$buffer, $offset );

If the stream contains a whole data block, this returns a new block which references the bytes
of the supplied C<$buffer>.  C<$offset> B<will be modified> to reflect the number of bytes
consumed.  Bytes may be consumed even if no block is returned.  This method causes changes to
the state of the stream object in addition to finding blocks.

=cut

sub parse_inplace {
	my ($self, $buffer_ref)= @_;
	defined $_[2] or croak 'Buffer $offset argument must be defined';
	if (!defined $self->bigendian) { # still working on the header
		if (length $$buffer_ref < $_[2]+32) {
			return undef unless $self->_eof;
			Userp::Error::EOF->throw(message => 'EOF while reading userp header')
		}
		pos($buffer_ref)= $_[2];
		$$buffer_ref =~ /\GUserp S([1-9]) (BE|LE)\0(..)(.{18})/
			or Userp::Error::Protocol->throw(message => 'Invalid Userp stream signature');
		$self->_set_proto_major($1);
		$self->_set_bigendian($2 eq 'BE'? 1 : 0);
		$self->_set_proto_minor(unpack(($self->bigendian? 'S>':'S<'), $3));
		utf8::decode(my $sig= $4);
		$self->_set_writer_sig($sig);
		$self->_want_scopeid(1);
		$_[2] += 32;
	}
	#eval {
	#	pos($$buffer_ref)= $_[2];
	#	my $scope_id= !$self->_want_scopeid? undef
	#		: $self->bigendian? Userp::Bits::read_vqty_be($$buffer_ref)
	#		: Userp::Bits::read_vqty_le($$buffer_ref)
	#	
	#my $dec= Userp::Decoder->new(buffer_ref => $buffer_ref);
	#my $scope_id= $self->_want_scopeid? $
	#	
	#}
}

sub next_block {
	my $self= shift;
	{
		my $block= $self->parse_inplace(\$self->{_buffer}, $self->{_offset});
		return $block unless !$block && $self->fh && !$self->_eof;
		# need more bytes.  Then try again
		$self->_read_more;
		redo;
	}
}

1;
