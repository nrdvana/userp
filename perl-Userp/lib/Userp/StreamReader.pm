package Userp::StreamReader;
use Moo;

=head1 SYNOPSIS

  # Blocking I/O Style
  my $st= Userp::StreamReader->new();
  while (my $block= $st->read($fh)) { # consume metadata blocks, return data blocks
    ...
  }

  # Event-Callback I/O Style
  my $st= Userp::StreamReader->new();
  ... => sub { # Event: new data becomes available on $buffer
    my $block= $st->parse($buffer);
    # $block is only defined if complete block has been read from $buffer
    # $block might be a MetadataBlock or DataBlock
    $buffer= ''; # was fully consumed
  }

  # Event-Callback Zerocopy I/O Style 
  my $st= Userp::StreamReader->new();
  ... => sub { # Event: new data becomes available on $buffer
    my ($block, $bytes_consumed)= $st->parse_inplace($buffer, $offset, $length);
    if ($block) {
      ... # decode $block fully before changing $buffer
    }
    $offset += $bytes_consumed;
  }

=head1 DESCRIPTION

This object reads the Userp Stream protocol.  Streams begin with a header, followed by metadata
blocks (which define types and string tables) and data blocks.  Each block must be fully read
into memory before it can be decoded.

The reader can operate in C<read>, C<parse>, or C<parse_inplace> mode.  C<read> is a blocking
read that does not return until it has read an entire block or until end of file.  C<parse> is
similar, but allows you to perform your own read (incluing via non-blocking methods) and then
deliver data in chunks.  The C<parse_inplace> method allows you to parse a buffer of data
without copying the buffer, however you must be sure not to alter the buffer while decoding the
block, and keep track of the current position within the buffer.

=head1 ATTRIBUTES

=over

=item bigendian

Whether the integers in the stream are written most-significant-byte-first.  This is detected
from the header of the stream on the first read, but is C<undef> until then.

=item version

The version of the streaming protocol.  This is detected from the header of the stream on the
first read, but is C<undef> until then.

=item bytes_read

Counter of number of bytes consumed from the stream.

=back

=cut

has bigendian     => ( is => 'rwp' );
has version       => ( is => 'rwp' );
has bytes_read    => ( is => 'rwp' );

=head1 METHODS

=head2 new

Standard L<Moo> constructor, but there are no settable attributes yet.

=head2 read

=head2 parse

=head2 parse_inplace

=cut

sub read {
}

sub parse {
}

sub parse_inplace {
}

1;
