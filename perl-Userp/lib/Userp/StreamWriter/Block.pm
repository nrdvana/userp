package Userp::StreamWriter::Block;
use Moo;

=head1 SYNOPSIS

  my $block= $writer->begin_block(%opts);
  $block->scope->add_symbol($sym1, ...);
  $block->scope->define_type($name, %spec);
  $block->set_meta(%extra_metadata_fields);
  $block->encoder->str("Hello World");

=head1 DESCRIPTION

This object represents a block that will be written or has been written to a Userp stream.
The object is mutable until the point that it gets written to the stream, after which attempts
to change it will throw an exception.

There is probably no reason to create blocks directly; use L<Userp::StreamWriter/new_block>
or L<Userp::StreamWriter/begin_block>

The canonical sequence to encode a block (requiring the least temporary data) is:

=over

=item Declare the size of the block

You are unlikely to know the size of your encoded data in advance, but the block may be larger
than the data, allowing you to seek back to the block and make changes later without needing
to re-write the entire stream.

Alternatively, if you set C<< $block->max_size($bytes) >> the library will reserve enough bytes
to describe an integer up to this maximum, then come back and fill it in once the actual length
of the data is known.

Without calling one of these methods, the library will create buffer fragments as needed and
combine them later.

=item Declare all Symbols

Use C<< $block->scope->add_symbol($string) >> for each symbol, including type names, field
names, integer named values, etc.

=item Declare all Types

Use C<< $block->scope->define_type(...) >> for each custom type used in your data.

=item Encode Metadata fields

Call C<< $block->metadata_encoder >> to get an encoder.  The C<current_type> of this encoder
will be C<'Userp.block_metadata'>, which is a record with some pre-defined fields and also
allowing ad-hoc fields.  Declare the count and names of all fields you will be encoding, then
encode each field.

=item Encode the Data

Call C<< $block->encoder >> to get the encoder for the data portion of the block.
The C<current_type> will be C<< $block->root_type >>, which defaults to C<'Userp.Any'> but can
be set as metadata in this block or in a parent.

=back

Those steps can be a bit tedious, though, so the library provides some helpful shortcuts:

=over

=item *

Any symbols needed by a type will be automatically added to the symbol table.

=item *

You may declare metadata key/value pairs with L</set_meta> instead of directly interacting with
the L</metadata_encoder>.  These will be written at the appropriate time, though errors in the
perl data with respect to the declared field types may throw exceptions at that time.

=item *

You may begin using the L</encoder> before the metadata has been written, and automatically add
new symbols and types as they are encountered.  The data will be written to a separate buffer
and appended to the metadata once the metadata is finalized (which implicitly happens when the
data is finalized)

=back

=head1 ATTRIBUTES

=head2 stream

Weak-reference to the stream where this block will be written.

=head2 parent

Reference to the block (if any) that defines the scope that this block inherits from.

=head2 scope

Instance of L<Userp::Scope> listing the symbols and types available for this block.  New types
and symbols can be added to the scope until the point where this block gets written to the
stream.

=head2 encoder

Instance of L<Userp::Encoder> on which you can call methods to add data to the block.

=head2 metadata_encoder

Instance of L<Userp::Encoder> on which you can encode a value of type C<'Userp.BlockMetadata'>.
It is an error to use this object if you provided metadata to L</set_meta>.

=head2 root_type

This pseudo-attribute returns the current C<'Userp.block_root_type'>.
Setting this attribute is a shortcut for C<< set_meta( 'Userp.block_root_type' => $type ) >>.

=head2 address

After the block is written, this holds the offset within the stream where the block started.
(i.e. the first byte following the stream control code)

=head2 size

Before the block is written, you can set this attribute to create a constant-length block.
If you do not set it, when the block is written this will be updated to the actual length
of the block, in bytes.  It is then read-only.

=head2 max_size

Before the block is written, you can set this attribute to allow the library to reserve space
for the encoded size value.  If the size ends up exceeding this value, the encoding will fail.
The default is C<undef>.

=cut

has stream           => ( is => 'ro', required => 1, weak_ref => 1 );
has parent           => ( is => 'ro', required => 1 );
has scope            => ( is => 'lazy' );
has encoder          => ( is => 'lazy' );
has metadata_encoder => ( is => 'lazy' );
has address          => ( is => 'rwp' );
sub size;
sub max_size;
sub root_type;
has _meta_attr       => ( is => 'rw' );
# Each element of _meta_attr looks like:
# {
#   name => # string
#   value => # representation of value as perl data
#   written => # boolean
# }
has _state           => ( is => 'rw' );
use constant {
	STATE_INIT => 0,
	STATE_WROTE_TABLES => 1,
	STATE_WROTE_META => 2,
	STATE_DATA => 3,
	STATE_DONE => 4
};

sub size {
	my $self= shift;
	if (@_) {
		croak "Can't change size after metadata started"
			unless $self->{_state} < STATE_WROTE_TABLES;
		$self->{size}= shift;
	}
	$self->{size};
}

sub max_size {
	my $self= shift;
	if (@_) {
		croak "Can't change max_size aftre metadata started"
			unless $self->{_state} < STATE_WROTE_TABLES;
		$self->{max_size}= shift;
	}
	$self->{max_size};
}

sub root_type {
	my $self= shift;
	if (@_) {
		$self->set_meta('Userp.block_root_type' => $type);
	}
	return $self->{root_type} ||= $self->parent? $self->parent->root_type : $self->scope->type_Any;
}

=head1 METHODS

=head2 set_meta

Queue perl data to be written as metadata fields.

=cut

sub set_meta {
	my $self= shift;
	croak "Metadata has already been written"
		unless $self->{_state} < STATE_WROTE_META;
	while (@_) {
		my ($k, $v)= splice(@_, 0, 2);
		if ($k eq 'Userp.block_root_type') {
			$self->{root_type}= $v;
		}
		$self->{_meta_attr}{$k}= $v;
	}
}

1;
