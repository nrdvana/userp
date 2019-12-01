package Userp::StreamWriter;
use Moo;

=head1 SYNOPSIS

  # Write a new stream
  my $st= Userp::StreamWriter->new(fh => $fh, %stream_options);
  my $block= $st->new_block(%block_options);
  $block->encoder->...  # call Encoder methods to construct block
  $block->write;
  
  # or, build a metadata block to optimize the encoding of blocks
  my $st= Userp::StreamWriter->new(fh => $fh, %stream_options);
  my $meta= $st->new_meta(%meta_options);
  my $block= $meta->new_block(%block_options);
  $meta->scope->... # define new types or symbols
  $block->encoder->...  # write data
  $meta->write; # metadata block must be written before data blocks
  $block->write;
  
  # Non-blocking I/O:
  my $st= Userp::StreamWriter->new(%stream_options);
  my $block= $st->new_block(%block_options);
  $block->encoder->...  # call Encoder methods to construct block
  $block->write(\my $buffer);
  # queue $buffer to be written

=head1 DESCRIPTION

This object writes the Userp Stream protocol.  Streams begin with a header, followed by metadata
blocks (which define types and string tables) and data blocks.  The protocol is written as a
series of blocks where each block is contained entirely in memory.  Because of this, you can
actually build several blocks in parallel before writing them out, such as building the symbol
table or set of common constants in a metadata block at the same time as building the blocks
which uses it.

=head1 ATTRIBUTES

=over

=item fh

File handle.  Must be writable.  This is optional if you prefer to pass a file handle to each
call to C<write>.

=item bigendian

Whether to write the protocol most-significant-byte-first.  If false (the default) the protocol
is written little-endian, giving a small speed boost to matching architectures.

=item version

The version of the streaming protocol.  Defaults to 1, unless a feature in the first block
forces it to use a higher version.

=item bytes_written

Counter of number of bytes written to the stream.

=back

=cut

has fh            => ( is => 'ro' );
has bigendian     => ( is => 'ro' );
has version       => ( is => 'rwp', default => 1 );
has bytes_written => ( is => 'rwp' );

=head1 METHODS

=head2 new

Standard L<Moo> constructor; Attribute C<fh> is required, and you can also specify C<bigendian>
and (in later versions) C<version>.

=head2 new_meta

Returns a new L<Userp::StreamWriter::MetadataBlock> which inherits and extends the root Scope
of the stream.  You may pass MetadataBlock constructor arguments.

=head2 new_block

Creates a new L<Userp::StreamWriter::DataBlock> in the root Scope of the stream.  You may pass
C<DataBlock> constructor arguments.

=cut

sub new_meta {
}

sub new_block {
}

