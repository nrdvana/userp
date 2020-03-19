package Userp::Encoder;
use Moo;
use Carp;
use Userp::Bits;
use Userp::MultiBuffer;
use Userp::Type::Record qw( ALWAYS OFTEN SELDOM );

=head1 SYNOPSIS

  # Encode Integer types
  $enc->int(42);
  
  # Encode symbolic constants
  $enc->sym('True');
  
  # Select a specific sub-type of the current type
  $enc->sel($sub_type);
  
  # Encode Arrays
  $enc->begin_array(3)->int(1)->int(2)->int(3)->end_array;
  
  # Encode records
  $enc->begin_record(qw/ foo bar /)->int(6)->int(2)->end_record;
  
  # Encode records with pre-known order of fields
  $enc->sel($my_rec_type)->begin_record->int(6)->int(2)->end_record;
  
  # Encode any type with a known optimized conversion from float
  $enc->float(4.2);
  
  # Encode any type with a known optimized conversion from string
  $enc->str("Hello World");
  
=head1 DESCRIPTION

This class is the API for serializing data.  The behavior of the encoder is highly dependent
on the L</scope>, which defines the available defined types and symbol table.

The sequence of API calls depends on type of the value being encoded.  Here is a brief
overview:

=over

=item Integer

Userp Integer types can have a constrained range, and optional symbols enumerating some of the
values.  Calling C<int> will interpret the perl scalar as an integer, check whether it is in
the domain of this Integer type, then encode it.  Calling C<sym> will look for a named value
within this type and then encode the integer.  If the integer is out of bounds, or if no such
symbol exists, the operation dies with an exception, and the encoder state remains unchanged.

  $enc->int($int_value);
  $enc->sym($symbolic_name);   # for integer types with named values

=item Symbol

If the current type is Symbol, you may encode any symbolic value that exists in the L</scope>'s
symbol table.  If the C<scope> is not finalized, you may encode any string value and it will
be automatically added to the symbol table.

  $enc->sym($symbolic_name);

=item Choice (or Any)

Choice types offer a selection between several types or values.  Select a more specific type
with the C<sel> method.  Select a specific option of the Choice type by passing two arguments;
the choice type and the index of the option you want.

  $enc->sel($type)->...    # select a member type
  $enc->sel(undef, 3)->... # select option 3 of the choice
  $enc->sel($type, 7)->... # select a sub-type and its option both at once

The library may allow you to skip calling C<sel> if there is a member type that can encode the
next thing you ask to encode.  For instance, if you call L</int> when the current type is
C<Userp.Any>, it will automatically select the first Integer type of C<Userp.Int>.

=item Array (and String)

Declare the dimensions of an array having dynamic dimensions during C<begin_array>.
Then, write each element of the array, iterating right-most dimension first.
(you might call this row-major order, but Userp does not distinguish which array index is
the "row")  Then call C<end_array>.

  $enc->begin_array($dim_1, ...); # only dimensions not specified in the type
  for(...) {
    $enc->...(...); # call once for each element
  }
  $enc->end_array();

Between C<begin_array> and C<end_array>, the C<current_type> will be the declared type of the
arra's elements, switching to C<undef> after the final element has been encoded.  You may not
call C<end_array> until all elements ar ewritten.

Userp has special handling for some array types, like C<Userp.String> or C<Userp.ByteArray>:

  $enc->str($string);

This avoids the obviously unacceptable performance of writing each individual byte with a
method call.

=item Record

Optionally declare the fields you will write, then encode each field in that order.
If the fields are defined in the type and have a static order, you can avoid fragmented buffers
by specifying those fields in that same order.

  $enc->begin_record("foo","bar");
  $enc->int(3);
  $enc->int(4);
  $enc->end_record;

If you do not want to pre-declare your fields, you must specify which field you are encoding
before each element.  However, this can result in more fragmented buffers than pre-declaring
the fields.

  $enc->begin_record;
  $enc->field("foo")->int(3);
  $enc->field("bar")->int(4);
  $enc->end_record;

=item Type

In very few cases, you might want to encode a reference to a type.  (i.e. encoding a reference
to a type as a value)  This gets its own method:

  $enc->typeref($type);

=back

=head1 ATTRIBUTES

=head2 scope

The current L<Userp::Scope> which defines what types and symbols are available.

=head2 bigendian

If true, all fixed-length integer values will be written most significant byte first.
The default on this object is false, but encoders created by L<Userp::StreamWriter> will be set
to whatever the stream is using.

=head2 buffers

Arrayref of L<Userp::Buffer> objects into which the data is encoded.

=head2 parent_type

The type whose elements are currently being written.  After you call L</begin_array> the
C<parent_type> is the Array and the C<current_type> is the type of the array element.
Likewise after calling L</begin_record>.

This does not change after calling L</sel>.

=head2 current_type

The type being written next.  This does change after calling C<sel>.

=cut

has scope        => ( is => 'ro', required => 1 );
has bigendian    => ( is => 'ro' );
has buffers      => ( is => 'rw' );
sub parent_type     { $_[0]{_parent}? $_[0]{_parent}{type} : undef }
sub current_type    { $_[0]{_current}{type} }

# After encoding an element, the _current encoder is set to this value, which has a undef $type
our $_final_state= Userp::Encoder::_Impl->new(undef);

sub BUILD {
	my ($self, $args)= @_;
	$self->buffers($self->bigendian? Userp::MultiBuffer->new_be : Userp::MultiBuffer->new_le)
		unless $self->buffers;
	if (defined (my $type= $args->{current_type})) {
		$type= $self->scope->find_type($type) || croak "No type $type in scope"
			unless ref $type && ref($type)->isa('Userp::Type');
		$self->{_current}= $self->_encoder_for_type($type)->new($type, $self);
	}
	else {
		$self->{_current}= $_final_state;
	}
}

our @PUBLIC_STATE_CHANGE_METHODS= qw(
	sel int sym str field float begin_array end_array begin_record end_record
);

=head1 METHODS

=head2 sel

  $enc->sel($type)->...
  $enc->sel(undef, $option)->...
  $enc->sel($type, $option)->...

Select a sub-type, option, or option-of-subtype of the current Choice.

Returns the encoder for convenient chaining.

=cut

sub sel {
	@_ == 2 or @_ == 3 or croak 'Expected one or two arguments to enc->sel($type, $option)';
	$_[0]{_current}->sel(@_);
	return $_[0];
}

=head2 int

  $enc->int($int_value)->... # plain scalar or BigInt object

The C<$int_value> must be an integer within the domain of the L</current_type>.
Method dies if arguments are invalid or if a non-integer value is required at this point of the
stream.  Returns the encoder for convenient chaining.

=cut

sub int {
	@_ == 2 or croak 'Expected one argument to enc->int($int)';
	$_[0]{_current}->int(@_);
	return $_[0];
}

=head2


=cut

sub sym {
	@_ == 2 or croak 'Expected one argument to enc->sym($string)';
	$_[0]{_current}->sym(@_);
	return $_[0];
}

=head2 str

  $enc->str($string)->...

Encode a string.  The type must be one of the known string types (i.e. Arrays of characters or
bytes), Symbol, an Integer with symbolic (enum) names, or a Choice including one of those.
If the type is 'Any', it will default to the first (lowest type id) string type in the current
Scope, else Symbol.

=cut

sub str {
	@_ == 2 or croak 'Expected one argument to enc->str($string)';
	$_[0]{_current}->str(@_);
	return $_[0];
}

=head2 typeref

  $enc->typeref($type)->... # Type object

=cut

sub typeref {
	@_ == 2 or croak 'Expected one argument to enc->typeref($type)';
	$_[0]{_current}->typeref(@_);
	return $_[0];
}

=head2 begin_array

  $enc->begin_array(
     $type,       # for arrays where ->elem_type is undefined 
	 @dimensions, # for arrays where any dimension was undefined
  );

Begin encoding the elements of an array.  The first argument is a type or type name
if (and only if) the array has an undefined C<elem_type>.

The remaining arguments are the dimensions of the array.  These must agree with any
declared dimensions on the array type.

=head2 end_array

Finish encoding the elements of the array, returning to the parent type, if any.

=cut

sub begin_array {
	$_[0]{_current}->begin_array(@_);
	return $_[0];
}

sub end_array {
	@_ == 1 or croak "Extra arguments to end_array";
	$_[0]{_parent} or croak "Can't end_array when there is no parent_type";
	$_[0]{_parent}->end_array(@_);
	return $_[0];
}

=head2 begin_record

  $enc->begin_record(@fields);

Begin encoding a record.  C<@fields> is optional; if given, you may encode each field in that
sequence without calling L</field> inbetween.  For highest efficiency, specify the list of
fields in the same order they are defined in the type, else the later fields will be encoded
to temporary buffers until the earlier fields are encoded.

=head2 end_record

Finish encoding the record and return to the parent type, if any.

=cut

sub begin_record {
	$_[0]{_current}->begin_record(@_);
	return $_[0];
}

sub end_record {
	$_[0]{_parent} or croak "Can't end_record when there is no parent_type";
	$_[0]{_parent}->end_record(@_);
	return $_[0];
}

#=============================================================================

sub _gen_Any_encoder {
	my ($self, $type)= @_;
	my $vqty_fn= Userp::Bits->can($self->bigendian? 'concat_vqty_be' : 'concat_vqty_le');
	my $align= $type->align;
	my $sel_fn= sub {
		# Verify this type is in scope
		$_[0]->scope->type_by_id($_[1]->id) == $_[1]
			or return Userp::Error::API->new(message => 'Type "'.$_[1]->name.'" is not in the current scope');
		# Encode the type selector
		$_[0]->_align($align);
		$vqty_fn->($_[0], $_[1]->id);
		# And now that type is the current type
		$_[0]->_set_current_type($_[1]);
		return;
	};
	my $int_fn= sub {
		my $int_t= $_[0]->scope->default_integer_type
			or return Userp::Error::API->new(message => 'No default Integer type specified in scope');
		$_[0]->_align($align);
		$vqty_fn->($_[0], $int_t->id);
		$_[0]->_vtable_for($int_t)->{int}->($_[0], $_[1]);
		return;
	};
	my $str_fn= sub {
		my $str_t= $_[0]->scope->default_string_type
			or return Userp::Error::API->new(message => 'No default String type specified in scope');
		$_[0]->_align($align);
		$vqty_fn->($_[0], $str_t->id);
		$_[0]->_vtable_for($str_t)->{str}->($_[0], $_[1]);
		return;
	};
	my $begin_fn= sub {
		my $seq_t= $_[0]->scope->default_sequence_type
			or return Userp::Error::API->new(message => 'No default sequence type (array/record) specified in scope');
		$_[0]->_align($align);
		$vqty_fn->($_[0], $seq_t->id);
		$_[0]->_vtable_for($seq_t)->{str}->($_[0], $_[1]);
		return;
	};
	return { sel => $sel_fn, int => $int_fn, str => $str_fn, begin => $begin_fn };
}

sub _encode_initial_scalar {
	my ($self, $scalar_value)= @_;
	my @to_encode;
	my $type= $self->current_type;
	my $val= $scalar_value;
	my $choice_path= $self->_choice_path;
	if (@$choice_path) {
		for (reverse @$choice_path) {
			defined (my $opt= $_->_option_for($type, $val))
				or croak "Options of Choice ".$_->name." do not include type ".$type->name." offset ".$val;
			$val= defined $opt->[1]? $val + $opt->[1]  # merged option, with adder
				: defined $opt->[2]? (($val << $opt->[2]) | $opt->[3])  # merged option, with shift/flags
				: do { # non-merged option, as single int.
					# Needs to be followed by encoding of $subtype, unless a single value was selected
					# from subtype.
					unshift @to_encode, [ $type, $val ] if $opt->[4] > 1;
					$opt->[0];
				};
			$type= $_;
		}
	}
	my $max= $type->scalar_component_max;
	$self->_encode_qty($val, defined $max? _bitlen($max) : undef);
	for (@to_encode) {
		$max= $_->[0]->scalar_component_max;
		$self->_encode_qty($_->[1], defined $max? _bitlen($max) : undef);
	}
	return $self;
}

# Update the buffer to meet the desired alignment, possibly adding
# a new buffer to the list.
sub _align {
	my ($self, $new_align)= @_;
	my $buf= $self->buffers->[-1];
	# If the desired alignment is less than the buffer, just add padding
	if ($new_align < $buf->alignment && $new_align > -3) {
		$buf->pad_to_alignment($new_align);
	}
	# Else need to create a new higher-aligned buffer.  The previous one will
	# get padded up to this alignment when it is time to get written.
	elsif ($new_align > $buf->alignment) {
		# If the buffer was not used yet, just change its declared alignment
		if (!$buf->length) {
			$buf->alignment= $new_align;
		} else {
			push @{ $self->buffers }, $buf->new_same(undef, $new_align);
		}
	}
	$self;
}

sub _push_state {
	my ($self, $type)= @_;
	$type= $self->scope->find_type($type) || croak "No type in scope: '$type'"
		unless ref $type;
	my $enc= ($type->{_encoder_cache} || $self->_encoder_for_type($type))->new($type, $self);
	$self->{_parent}= Userp::Encoder::_TempState->new($type, $self);
	$self->{_current}= $enc;
}

sub _encoder_for_type {
	my ($self, $type)= @_;
	$type->{_encoder_cache} ||= (
		$type->parent? $self->_encoder_for_type($type->parent)
		: 'Userp::Encoder::_'.$type->base_type
		)->specialize($type);
}

# Encode a reference to a symbol or type from a scope's table
sub _get_table_ref_int {
	my ($self, $scope_idx, $elem_idx)= @_;
	die "BUG: scope_idx=$scope_idx elem_idx=$elem_idx"
		unless defined $elem_idx && defined $scope_idx;
	# scope_idx[-1] is encoded as a shift of 0
	# scope_idx[0] is encoded as a shift of 1
	# scope_idx[-2] is encoded as a shift of 2
	# scope_idx[1] is encoded as a shift of 3
	# and so on.
	my $scope_max= $self->scope->scope_idx;
	my $shift= $scope_idx > $scope_max/2? 2 * ($scope_max - $scope_idx) : 1 + 2 * $scope_idx;
	return (($elem_idx << 1) + 1) << $shift;
}

sub _encode_type_ref {
	my ($self, $type)= @_;
	$self->buffers->[-1]->encode_int(
		!defined $type? 0
		: $self->_get_table_ref_int($type->scope_idx, $type->table_idx)
	);
}

sub _encode_symbol_ref {
	my ($self, $sym)= @_;
	$self->buffers->[-1]->encode_int(
		!defined $sym? 0
		: $self->_get_table_ref_int($self->scope->find_symbol($sym))
	);
}

# Encode the symbol table and type table at the start of a block
sub _encode_block_tables {
	my $self= shift;
	$self->scope->final(1);
	my $buf= $self->buffers->[-1];
	croak "Type stack is not empty"
		if @{ $self->_enc_stack };
	
	# Need to encode bytes, not strings, so make a copy
	my @symbols= @{ $self->scope->_symbols };
	utf8::encode($_) for @symbols;
	# Number of entries in the symbol table
	$buf->encode_int(scalar @symbols);
	# length of each entry, not including NUL terminator
	$buf->encode_int(length $_) for @symbols;
	# Symbol data
	$buf->encode_bytes($_)->encode_bytes("\0") for @symbols;
	
	# Number of entries in the type table
	my @types= @{ $self->scope->_types };
	$buf->encode_int(scalar @types);
	for (@types) {
		# Encode reference to parent type, which determines how fields are encoded
		$self->_encode_type_ref($_->parent);
		# Use class of type to delegate remainder of encoding
		$_->_encode_definition($self);
		croak "Incomplete type encoding"
			if defined $self->current_type || @{ $self->_enc_stack };
	}
	# Tables are complete
	return $self;
}

# Encode type definition for Any, Symbol, or Type, each of which only has name/parent/metadata
sub _encode_definition_simple {
	my ($type, $self)= @_;
	my $attrs= $type->get_definition_attributes;
	$self->_prepare_typedef_metadata($attrs);
	$self->_push_temporary_state('Userp.Typedef.Simple');
	$self->encode($attrs);
}
*Userp::Type::Any::_encode_definition= *_encode_definition_simple;
*Userp::Type::Symbol::_encode_definition= *_encode_definition_simple;
*Userp::Type::Type::_encode_definition= *_encode_definition_simple;

# TODO: maybe optimize the _encode_definition_simple? Could be much more efficient,
# but not 'hot' code at all...
#
#	$self->_encode_symbol_ref($type->name);
#	# Encode reference to parent type
#	$self->_encode_type_ref($type->parent);
#	# There is only one field for Type::Any.  If it is not used, then the "no attributes" case
#	# can be optimized as a single NUL byte.
#	my $meta= $type->metadata;
#	if ($meta && keys %$meta) {
#		$self->_push_temporary_state('Userp.Typedef.Simple');
#		$self->begin_record('metadata');
#		$self->_encode_typedef_metadata($meta);
#		$self->end_record;
#	} else {
#		$self->buffers->[-1]->encode_int(0, 1);
#	}
#}

sub Userp::Type::Integer::_encode_definition {
	my ($type, $self)= @_;
	my $attrs= $type->get_definition_attributes;
	# Convert from logical "set of names" to structure used by definition
	$self->_prepare_integer_names($attrs);
	# Convert metadata to a buffer of bytes, or Userp::PendingAlignment
	$self->_prepare_typedef_metadata($attrs);
	$self->_push_temporary_state('Userp.Typedef.Integer');
	$self->encode($attrs);
}

sub Userp::Type::Choice::_encode_definition {
	my ($type, $self)= @_;
	my $attrs= $type->get_definition_attributes;
	# Convert literal values to a buffer of bytes, or Userp::PendingAlignment
	$self->_prepare_choice_options($attrs);
	# Convert metadata to a buffer of bytes, or Userp::PendingAlignment
	$self->_prepare_typedef_metadata($attrs);
	$self->_push_temporary_state('Userp.Typedef.Choice');
	$self->encode($attrs);
}

sub Userp::Type::Array::_encode_definition {
	my ($type, $self)= @_;
	$self->_push_temporary_state('Userp.Typedef.Array');
	$self->encode($type->get_definition_attributes);
}

sub Userp::Type::Record::_encode_definition {
	my ($type, $self)= @_;
	$self->_push_temporary_state('Userp.Typedef.Record');
	$self->encode($type->get_definition_attributes);
}

# Convert a hash of { $name => $value } to a list of record-or-symbol:
# [ 
#   { name => $name, val => $val },
#   $name,
#   $name,
#   ...
#   { name => $name, val => $val },
#   ...
# ]
# where each name in a group is the previous plus one.
sub _prepare_integer_names {
	my ($self, $attrs)= @_;
	my $names= $attrs->{names} or return;
	my @finished;
	my @active; # a array of arrays where the last element is a number of what can take its place
	# Sort list by value
	name: for my $name (sort { $names->{$a} <=> $names->{$b} } keys %$names) {
		my $val= $names->{$name};
		# For each list actively being built, see if this new element fits on the end
		for (my $i= 0; $i < @active; ++$i) {
			if ($active[$i][-1] == $val) {
				$active[$i][-1]= $name;
				push @{$active[$i]}, $val+1;
				next name;
			}
			# Commit any lists that ended prior to this value.
			elsif ($active[$i][-1] < $val) {
				push @finished, splice(@active, $i, 1);
				--$i;
			}
		}
		# No list could be appended.  Create a new one.
		push @active, [ $name, $val+1 ];
	}
	push @finished, @active;
	# Each list is a list of names followed by the next value, meaning that the value of
	# the first element is the "next" value minus the number of names in the list.
	my @result;
	for (@finished) {
		my $v= pop @$_;
		$v -= @$_;
		$_->[0]= { name => $_->[0], value => $v }
			unless !@result && !$v; # very first element does not need to be a record if val is 0
		push @result, @$_;
	}
	$attrs->{names}= \@result;
}

sub _prepare_choice_options {
	my ($self, $attrs)= @_;
	my $options= $attrs->{options} or return;
	...;
}

# Convert metadata to a buffer of bytes, or Userp::PendingAlignment
sub _prepare_typedef_metadata {
	my ($self, $attrs)= @_;
	my $meta= $attrs->{metadata} or return;
	# Start a new buffer marked as byte-align.  If the encoding of the metadata needs greater
	# alignment, it will create additional buffers.
	my $buf_idx= $#{ $self->buffers };
	my $buf= $self->buffers->[-1];
	push @{ $self->buffers }, $buf->new_same(undef, 0);
	# Encode the metadata as a generic record
	$self->_push_state($self->scope->type_Record);
	$self->encode($meta);
	# Remove the temporary buffers
	my @new_bufs= splice @{$self->buffers}, $buf_idx+1;
	# If the overall alignment of the new buffers is still zero, then they can simply be passed
	# to later encoding as a byte array.
	if (@new_bufs == 1 && $new_bufs[0]->alignment == 0) {
		$attrs->{metadata}= $new_bufs[0];
	}
	# Else things get complicated because the length depends on how the
	# buffers get padded during alignment
	else {
		$attrs->{metadata}= Userp::PendingAlignment->new(buffers => \@new_bufs);
	}
}

#sub _prepare_typedef_metadata {
#	my ($self, $meta)= @_;
#	# Metadata is tricky, because it is written as an array of bytes which happens to contain
#	# an encoded record.  However, the alignment of that encoding needs to be relative to
#	# the overall stream.
#	# Start a new buffer marked as byte-align.  If the encoding of the metadata needs greater
#	# alignment, it will create additional buffers.
#	my $buf_idx= $#{ $self->buffers };
#	my $buf= $self->buffers->[-1];
#	push @{ $self->buffers }, $buf->new_same(undef, 0);
#	# Encode the metadata as a generic record
#	$enc->_push_temporary_state($self->scope->type_Record);
#	$enc->encode($meta);
#	# If every new buffer has alignment less than the current, collapse them
#	# back to a single buffer.  (alignment will only increase among @new_bufs)
#	if ($self->buffers->[-1]->alignment <= $buf->alignment) {
#		# Note that the most likely scenario here is that metadata only had strings
#		# or variable ints in it, so it only needed byte alignment, so it all went
#		# into a single buffer.  So... optimize that case.
#		my @new_bufs= splice @{$self->buffers}, $buf_idx+1;
#		if (@new_bufs == 1 and $new_bufs[0]->alignment == 0) {
#			$buf->encode_int($len)->encode_bytes(${ $new_bufs[0]->bufref });
#		}
#		else {
#			my $buf_pos= $buf->length;
#			# Sum it up assuming best alignment padding, then append a length int.
#			my $len= 0;
#			$len += $_->length for @new_bufs;
#			$buf->encode_int($len);
#			# Now calculate the real length with alignment
#			{
#				my $real_len= $buf->length << 3;
#				for (@new_bufs) {
#					$real_len= Userp::Bits::roundup_bits_to_alignment($real_len, $_->alignment);
#					$real_len += $_->length << 3;
#				}
#				$real_len= (($real_len + 7) >> 3) - $buf->length;
#				if ($real_len != $len) {
#					my $buf_len= $buf->length;
#					# remove the length previously written
#					substr(${$buf->bufref}, $buf_pos)= '';
#					$buf->encode_int($real_len);
#					# and then re-calculate the length-with-alignment if the int changed length
#					redo if $buf_len != $buf->length;
#				}
#			}
#			# Then concatenate (with padding) all the new buffers
#			$buf->append_buffer($_) for @new_bufs;
#		}
#	}
#	# Else the length can't be known until there is more alignment available,
#	# so add a placeholder.
#	else {
#		die "Unimplemented: large alignment in metadata";
#		splice @{ $self->buffers }, $buf_idx+1, 0,
#			Userp::LazyLength->new($#{ $self->buffers } - $buf_idx);
#	}
#	# New state is that we have finished the "meta" field, and move to the next field.
#	$self->_next;
#}

# specialize is a class method that returns a class name most appropriate for the given type
sub Userp::Encoder::_Impl::specialize {
	# my ($class, $type)= @_;
	return $_[0];
}
# Create a new encoder state.  Default just captures the type, which is sufficient for most
sub Userp::Encoder::_Impl::new {
	my ($class, $type, $encoder)= @_;
	bless { type => $type }, $class;
}

# Install a default "can't do that" handler for each of the user-facing methods
sub _invalid_state {
	my ($self, $method)= @_;
	if (my $t= $self->current_type) {
		my $name= $t->name;
		my $type= $t->base_type;
		croak "Can't call '$method' when encoder is expecting type ".(defined $name? "$name ($type)" : $type);
	} elsif ($self->{_parent} && $self->{_parent}{type}) {
		croak "Can't call '$method' when expecting ".($self->{_parent}{type}->isa_Array? 'end_array':'end_record');
	} else {
		croak "Can't call '$method' after encoder reaches a final state";
	}
}

for (@PUBLIC_STATE_CHANGE_METHODS) {
	my $m= $_;
	no strict 'refs';
	*{'Userp::Encoder::_Impl::'.$m}= sub { _invalid_state($_[1], $m) };
}

# Encoder for temporarily changing encoder to a new state
@Userp::Encoder::_TempState::ISA= ( 'Userp::Encoder::_Impl' );

sub Userp::Encoder::_TempState::new {
	my ($class, $type, $encoder)= @_;
	bless { _parent => $encoder->{_parent}, _current => $encoder->{_current} }, $class;
}

sub Userp::Encoder::_TempState::next_elem {
	my ($state, $self)= @_;
	# Restore state captured by constructor
	$self->{_parent}= $state->{_parent};
	$self->{_current}= $state->{_current};
}

# Encoder for symbol references
@Userp::Encoder::_Symbol::ISA= ( 'Userp::Encoder::_Impl' );

sub Userp::Encoder::_Symbol::sym {
	my ($state, $self, $value)= @_;
	my ($scope_idx, $table_idx)= $self->scope->find_symbol($value)
		or croak "Symbol '$value' is not in scope";
	$self->buffers->[-1]->encode_int($self->_get_table_ref_int($scope_idx, $table_idx));
	$self->{_current}= $_final_state;
	$self->{_parent}->next_elem($self) if $self->{_parent};
}

*Userp::Encoder::_Symbol::str= *Userp::Encoder::_Symbol::sym;

# Encoder for type references
@Userp::Encoder::_Type::ISA= ( 'Userp::Encoder::_Impl' );

sub Userp::Encoder::_Type::typeref {
	my ($state, $self, $value)= @_;
	if (!ref $value) {
		$value= $self->scope->find_type($value)
			or croak "Type '$value' is not in scope";
	}
	$self->scope->contains_type($value)
		or croak "Type '$value' is not in scope";
	$self->buffers->[-1]->encode_int($self->_get_table_ref_int($value->scope_idx, $value->table_idx));
	$self->{_current}= $_final_state;
	$self->{_parent}->next_elem($self) if $self->{_parent};
}

# Encoder for generic Integers
@Userp::Encoder::_Integer::ISA= ( 'Userp::Encoder::_Impl' );

sub Userp::Encoder::_Integer::int {
	my ($state, $self, $value)= @_;
	my $type= $state->{type};
	my $min= $type->effective_min;
	my $max= $type->effective_max;
	my $bits= $type->effective_bits;
	die Userp::Error::Domain->new_minmax($value, $min, $max, 'Type "'.$type->name.'" value')
		if (defined $min && $value < $min) || (defined $max && $value > $max);
	my $align= $type->align;
	$self->_align($align) if defined $align;
	# If a type has a declared minimum value, it is encoded as an unsigned value from that offset.
	if (defined $type->min) {
		$value += $min;
	}
	# If the type has a max but not a min, it is encoded as an unsigned offset in the negative direction
	elsif (defined $type->max) {
		$value= $max - $value;
	}
	elsif (defined $type->bits) {
		$value= Userp::Bits::truncate_to_bits($value, $bits);
	}
	# If the type is fully unbounded, encode nonnegative integers as value * 2, and negative integers
	# as value * 2 + 1
	elsif (!defined $bits) {
		# TODO: check for overflow
		$value= $value < 0? (-$value * 2 + 1) : ($value * 2);
	}
	$self->buffers->[-1]->encode_int($value, $bits);
	$self->{_current}= $_final_state;
	$self->{_parent}->next_elem($self) if $self->{_parent};
}

sub Userp::Encoder::_Integer::sym {
	my ($state, $self, $value)= @_;
	my $names= $self->current_type->names;
	defined $value or croak "Can't look up enum value for undefined symbol";
	defined $names && exists $names->{$value}
		or croak "Integer Type ".($self->current_type->name || '')." does not have an enumeration for $value";
	$state->int($self, $names->{$value});
}

*Userp::Encoder::_Integer::str = *Userp::Encoder::_Integer::sym;

# Encoder for generic arrays
@Userp::Encoder::_Array::ISA= ( 'Userp::Encoder::_Impl' );

sub Userp::Encoder::_Array::begin_array {
	my $state= shift;
	my $self= shift;
	my $array= $state->{type};
	my $elem_type= $array->elem_type || shift;
	my @dim= @_;
	
	$state->{done}
		and croak "Expected end_array";
	
	if (!ref $elem_type || !ref($elem_type)->isa('Userp::Type')) {
		defined $elem_type
			or croak "Expected element type as first argument to begin_array";
		$elem_type= $self->scope->find_type($elem_type)
			|| croak "No such type '$elem_type' in current scope (first argument to begin_array must be a type or type name)";
	} else {
		$self->scope->contains_type($elem_type)
			or croak "Type ".$elem_type->name." is not part of current scope";
	}
	my @typedim= @{$array->dim || []};
	# If the type specifies some dimensions, make sure they agree with supplied dimensions
	if (@typedim) {
		croak "Supplied ".@dim." dimensions for type ".$array->name." which only defines ".@typedim." dimensions"
			unless @dim <= @typedim;
		# The dimensions given must match or fill-in spots in the type-declared dimensions
		for (my $i= 0; $i < @typedim; $i++) {
			$dim[$i]= $typedim[$i] unless defined $dim[$i];
			croak "Require dimension $i for type ".$array->name
				unless defined $dim[$i];
			croak "Dimension $i of array disagrees with type ".$array->name
				if defined $typedim[$i] && $dim[$i] != $typedim[$i];
		}
	}
	$state->{elem_type}= $elem_type;
	# Encode the elem_type unless part of the array type
	$self->_encode_type_ref($elem_type) unless $array->elem_type;
	# Encode the number of dimensions unless part of the array type
	$self->buffers->[-1]->encode_int(scalar @dim) unless @typedim;
	# Encode the list of dimensions, for each that wasn't part of the array type
	my $array_dim_enc= $array->dim_type? $self->_encoder_for_type($array->dim_type)->new($array->dim_type) : undef;
	my $n= 1;
	for (0..$#dim) {
		$n *= $dim[$_]; # TODO: check overflow
		next if $typedim[$_];
		$array_dim_enc? $array_dim_enc->int($dim[$_]) : $self->buffers->[-1]->encode_int($dim[$_]);
	}
	$state->{n}= $n; # now store actual number of array elements pending
	$state->{i}= 0;
	$state->{elem_encoder}= $self->_encoder_for_type($elem_type);
	$state->{_parent}= $self->{_parent};
	$self->{_parent}= $state;
	$self->{_current}= $state->{elem_encoder}->new($elem_type);
}

sub Userp::Encoder::_Array::next_elem {
	my ($state, $self)= @_;
	if (++$state->{i} < $state->{n}) {
		$self->{_current}= $state->{elem_encoder}->new($state->{elem_type});
	}
	# else remain at $_final_state
}

sub Userp::Encoder::_Array::end_array {
	my ($state, $self)= @_;
	defined $state->{i}
		or croak "Expected begin_array before end_array";
	my $remain= $state->{n} - $state->{i};
	croak "Expected $remain more elements before end_array"
		if $remain;
	$self->{_current}= $_final_state;
	$self->{_parent}= $state->{_parent};
	$self->{_parent}->next_elem($self) if $self->{_parent};
}

# Encoder for records
@Userp::Encoder::_Record::ISA= ( 'Userp::Encoder::_Impl' );
@Userp::Encoder::_RecordUndeclared::ISA= ( 'Userp::Encoder::_Record' );
@Userp::Encoder::_RecordDeclared::ISA= ( 'Userp::Encoder::_Record' );

sub Userp::Encoder::_Record::begin_record {
	my ($state, $self, @fields)= @_;
	# If user supplies list of fields, they intend to encode the fields in that order.
	# The fields with static, always, and often placement can only be encoded in
	# the order they are declared, so if that order differs from the order the user
	# wants, need to hold onto those encoded values until later.
	
	my $t= $state->{type};
	my $f_by_name= $t->_field_by_name;
	my $f_extra_type= $t->extra_field_type;
	my $often_field_mask= 0;
	my @often_field_list;
	my @dyn_field_list;
	my @dyn_field_codes;
	my %seen;
	# list of fixed-order fields, must be written before any dynamic fields.
	my @pending_ordered= $t->_ordered_field_list;
	if (@fields) {
		$seen{$_}++ && croak "Record field '$_' specified more than once"
			for @fields;
		@fields= map +($f_by_name->{$_} || $_), @fields;
		# Reduce the fixed-order fields to only the ones seen
		@pending_ordered= grep {
				$seen{$_->name}
				or $_->placement == ALWAYS && croak "Must specify required field '".$_->name."'"
			} @pending_ordered;
		%seen= (); # reset to track which fields came first
	}
	# If there are OFTEN fields, then initialize the bit mask
	Scalar::Util::weaken($state);
	my $queue_next_field= sub {
		defined( my $f= $_[0] ) or croak "Field ref or name required";
		$f= $f_by_name->{$f} unless ref $f;
		my $name= $f? $f->name : $_[0];
		!$seen{$name}++ or croak "Record field '$name' specified more than once";
		if (!$f) {
			defined $f_extra_type
				or croak "Record '".$t->name."' does not have a field '$name' and does not allow extra fields";
			my ($scope_idx, $sym_idx)= $self->scope->find_symbol($name)
				or croak "Symbol '$name' is not in scope";
			push @dyn_field_list, $name;
			push @dyn_field_codes, $t->_seldom_count + ((($sym_idx<<1)+1)<<$scope_idx);
			# ( name, type, is_discontinuity )
			# It's a discontinuity if any of the "Always" fields are not encoded yet,
			# or if any of the "Often" fields might still be encoded
			return ($name, $f_extra_type, scalar @pending_ordered);
		}
		# If a declared field is SELDOM, it gets added to the dynamic fields as well
		elsif ($f->placement == SELDOM) {
			push @dyn_field_list, $name;
			push @dyn_field_codes, $f->idx - $t->_seldom_ofs;
			# ( name, type, is_discontinuity )
			# It's a discontinuity if any of the "Always" fields are not encoded yet,
			# or if any of the "Often" fields might still be encoded
			return ($name, $f->type, scalar @pending_ordered);
		}
		# If a declared field is OFTEN, set the bit in the mask
		elsif ($f->placement == OFTEN) {
			my $is_next= @pending_ordered && $f == $pending_ordered[0];
			shift @pending_ordered if $is_next; #while @pending_ordered && $seen{$pending_ordered[0]->name};
			my $bit= $f->idx - $t->_often_ofs;
			$often_field_mask |= ($bit > 31? Math::BigInt->bone->blsft($bit) : 1 << $bit);
			# ( name, type, is_discontinuity )
			# It's a discontinuity if is_next is false
			return ($name, $f->type, !$is_next);
		}
		# If a declared field is ALWAYS, it gets cached if it wasn't in order
		elsif ($f->placement == ALWAYS) {
			my $is_next= @pending_ordered && $f == $pending_ordered[0];
			shift @pending_ordered if $is_next; #while @pending_ordered && $seen{$pending_ordered[0]->name};
			# ( name, type, is_discontinuity )
			# It's a discontinuity if is_next is false
			return ($f->name, $f->type, !$is_next);
		}
		elsif ($f->placement >= 0) {
			return ($name, $f->type, 0);
		}
		else { die "Bug: unhandled placement" }
	};
	
	$state->{_parent}= $self->{_parent};
	$self->{_parent}= $state;
	$state->{first_buf}= $self->buffers->[-1];
	$state->{pending_ordered}= \@pending_ordered;
	$state->{dyn_field_list}= \@dyn_field_list;
	$state->{dyn_field_codes}= \@dyn_field_codes if $t->_seldom_count || $t->extra_field_type;
	$state->{often_field_mask_ref}= \$often_field_mask;
	if (@fields) {
		$state->{queue}= [ map $queue_next_field->($_), @fields ];
		bless $state, 'Userp::Encoder::_RecordDeclared';
		$state->_write_record_intro();
	}
	else {
		$state->{queue_next}= $queue_next_field;
		bless $state, 'Userp::Encoder::_RecordUndeclared';
		# Can't write record intro until the fields are all known
	}
	$state->next_elem($self);
}

sub Userp::Encoder::_Record::_write_record_intro {
	my ($state, $self)= @_;
	my $buf= $state->{first_buf};
	my $t= $state->{type};
	# If there are SELDOM or EXTRA fields, write the count of them
	# followed by the code for each.
	if (my $dyn_fields= $state->{dyn_field_codes}) {
		$buf->encode_int(scalar @$dyn_fields);
		$buf->encode_int($_) for @$dyn_fields;
	}
	# If there are OFTEN fields, write out a bit vector (as an int)
	# indicating presence or absence of each.
	$buf->encode_int(${$state->{often_field_mask_ref}}, $t->_often_count)
		if $t->_often_count;
}

sub Userp::Encoder::_Record::_flush_fields {
	my ($state, $self)= @_;
	# Clean up after the previous field has been encoded
	if ($state->{out_of_order}) {
		# capture any out-of-order encoded field from previous
		$state->{encoded_field}{$state->{field}}= pop @{ $self->buffers }
	} elsif (keys %{$state->{encoded_field}}) {
		# ensure all previous out_of_order which should follow this one are written
		while (@{ $state->{pending_ordered} }) {
			my $buf= delete $state->{encoded_field}{$state->{pending_ordered}[0]->name}
				or last;
			shift @{ $state->{pending_ordered} };
			push @{ $self->buffers }, $buf;
		}
		# If wrote all of the pending_ordered, then start writing the dynamic ones too, until caught up
		if (!@{ $state->{pending_ordered} }) {
			for (@{ $state->{dyn_field_list} }) {
				my $buf= delete $state->{encoded_field}{$_}
					or last;
				push @{ $self->buffers }, $buf;
			}
		}
	}
}

sub Userp::Encoder::_RecordDeclared::next_elem {
	my ($state, $self)= @_;
	$state->_flush_fields($self);
	# See if there is another field to prepare
	if (($state->{field}, my $type, $state->{out_of_order})= splice(@{$state->{queue}}, 0, 3)) {
		$self->{_current}= $self->_encoder_for_type($type)->new($type);
		push @{ $self->buffers }, $self->buffers->[-1]->new_same(undef, -3)
			if $state->{out_of_order};
	}
	else {
		# else remain at $_final_state
		# All buffered fields should have gotten appended by here.
		!keys %{ $state->{encoded_field} }
			or die "Bug: leftover encoded fields";
	}
}

sub Userp::Encoder::_RecordDeclared::field {
	my ($state, $self, $name)= @_;
	croak "Field '$name' is not the next field (as declared during begin_record)"
		unless defined $state->{field} && $name eq $state->{field};
}

sub Userp::Encoder::_RecordUndeclared::next_elem {
	my ($state, $self)= @_;
	$state->_flush_fields($self);
	$state->{field}= undef;
	# RecordUndeclared can be used as its own sentinel to emit "no declared field" errors.
	# or to automatically select the next required field.
	$self->{_current}= $state;
}

sub Userp::Encoder::_RecordUndeclared::field {
	my ($state, $self, $name)= @_;
	croak "Can't declare next field until ".$state->{field}." is written"
		if defined $state->{field};
	($state->{field}, my $type, $state->{out_of_order})= $state->{queue_next}->($name);
	$self->{_current}= $self->_encoder_for_type($type)->new($type);
	push @{ $self->buffers }, $self->buffers->[-1]->new_same(undef, -3)
		if $state->{out_of_order};
}

# If user didn't declare fields and calls any encoder method,
# default the current field to the next ALWAYS field (if any)
# else throw an error.
for (grep $_ ne 'end_record' && $_ ne 'field', @PUBLIC_STATE_CHANGE_METHODS) {
	my $m= $_;
	my $fn= sub {
		my ($state, $self)= @_;
		if (@{$state->{pending_ordered}}) {
			$state->field($self, $state->{pending_ordered}[0]->name);
			# trampoline to new handler
			shift;
			$self->{_current}->$m(@_);
		}
		else {
			croak 'No declared field; specify one with ->field($name)';
		}
	};
	no strict 'refs';
	*{'Userp::Encoder::_RecordUndeclared::'.$_}= $fn;
}

sub Userp::Encoder::_RecordDeclared::end_record {
	my ($state, $self)= @_;
	if (@{$state->{queue}}) {
		croak "Expected field '$state->{queue}[0]' before end_record";
	}
	keys %{$state->{encoded_field}} and die "Bug: leftover fields unwritten";
	$self->{_current}= $_final_state;
	$self->{_parent}= $state->{_parent};
	$self->{_parent}->next_elem($self) if $self->{_parent};
}

sub Userp::Encoder::_RecordUndeclared::end_record {
	my ($state, $self)= @_;
	croak "Can't end_record until field '".$state->{field}."' is written"
		if defined $state->{field};
	if (my @always= grep $_->placement == ALWAYS, @{$state->{pending_ordered}}) {
		croak "The following required fields were not provided: ".join(', ', map $_->name, @always);
	}
	$state->_write_record_intro();
	$state->{out_of_order}= 0;
	$state->_flush_fields($self);
	keys %{$state->{encoded_field}} and die "Bug: leftover fields unwritten";
	$self->{_current}= $_final_state;
	$self->{_parent}= $state->{_parent};
	$self->{_parent}->next_elem($self) if $self->{_parent};
}

1;
