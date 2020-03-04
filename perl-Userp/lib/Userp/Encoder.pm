package Userp::Encoder;
use Moo;
use Carp;
use Userp::Bits;

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
  $enc->begin_record->field('foo')->int(6)->field('bar')->int(2)->end_record;
  
  # Encode records with known order of fields
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
has buffers      => ( is => 'rw', lazy => 1, builder => 1 );
sub parent_type     { $_[0]{_parent}? $_[0]{_parent}{type} : undef }
sub current_type    { $_[0]{_current}{type} }

sub _build_buffers {
	my $self= shift;
	[ $self->bigendian? Userp::Buffer->new_be : Userp::Buffer->new_le ]
}

=head1 METHODS

=head2 sel

  $enc->sel($type)->...
  $enc->sel(undef, $option)->...
  $enc->sel($type, $option)->...

Select a sub-type, option, or option-of-subtype of the current Choice.

Returns the encoder for convenient chaining.

=cut

sub sel {
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
	@_ == 2 or croak "Expected one argument to enc->int(\$int)";
	$_[0]{_current}->int(@_);
	return $_[0];
}

=head2


=cut

sub sym {
	@_ == 2 or croak "Expected one argument to enc->sym(\$string)";
	$_[0]{_current}->int(@_);
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
	@_ == 2 or croak "Expected one argument to enc->str(\$string)";
	$_[0]{_current}->str(@_);
	return $_[0];
}

=head2 typeref

  $enc->typeref($type)->... # Type object

=cut

sub typeref {
	@_ == 2 or croak "Expected one argument to enc->typeref(\$type)";
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
	push @{ $self->_enc_stack }, ($type->{_encoder_cache} || $self->_encoder_for_type($type))->new($type, $self);
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
	my $attrs= $type->get_definition_attributes
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
	$enc->_push_temporary_state($self->scope->type_Record);
	$enc->encode($meta);
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

sub _invalid_state {
	my ($self, $method)= @_;
	if (my $t= $self->current_type) {
		my $name= $t->name;
		my $type= $t->base_type;
		croak "Can't call '$method' when encoder is expecting type ".(defined $name? "$name ($type)" : $type);
	} else {
		croak "Can't call '$method' after encoder reaches a final state";
	}
}

for (qw( int str sym sel field float begin_array end_array begin_record end_record )) {
	no strict 'refs';
	my $m= $_;
	*{'Userp::Encoder::_Impl::'.$m}= sub { Userp::Encoder::_invalid_state($_[1], $m) };
}

our $final_state= bless {}, 'Userp::Encoder::_Impl';

sub Userp::Encoder::_Impl::specialize {
	return $_[0];
}

# Encoder for generic Integers
@Userp::Encoder::_Integer::ISA= ( 'Userp::Encoder::_Impl' );
sub Userp::Encoder::_Integer::new {
	my ($class, $type, $encoder)= @_;
	bless { type => $type }, $class;
}

sub Userp::Encoder::_Integer::int {
	my ($state, $self, $value)= @_;
	my $type= $state->{type};
	my $min= $type->effective_min;
	my $max= $type->effective_max;
	my $bits= $type->effective_bits;
	die Userp::Error::Domain->new_minmax($value, $min, $max, 'Type "'.$type->name.'" value')
		if $value < $min || $value > $max;
	my $align= $type->align;
	$self->_align($align) if defined $align;
	# If a type has a declared minimum value, it is encoded as an unsigned value from that offset.
	if (defined $min) {
		$value += $min;
	}
	# If the type has a max but not a min, it is encoded as an unsigned offset in the negative direction
	elsif (defined $max) {
		$value= $max - $value;
	}
	# If the type is fully unbounded, encode nonnegative integers as value * 2, and negative integers
	# as value * 2 + 1
	elsif (!defined $bits) {
		# TODO: check for overflow
		$value= $value < 0? (-$value * 2 + 1) : ($value * 2);
	}
	$self->buffers->[-1]->encode_int($value, $bits);
	$self->{_current}= $final_state;
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
sub Userp::Encoder::_Array::new {
	my ($class, $type, $encoder)= @_;
	bless { type => $type }, $class;
}
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
	# else remain at $final_state
}

sub Userp::Encoder::_Array::end_array {
	my ($state, $self)= @_;
	defined $state->{i}
		or croak "Expected begin_array before end_array";
	my $remain= $state->{n} - $state->{i};
	croak "Expected $remain more elements before end_array"
		if $remain;
	$self->{_current}= $final_state;
	$self->{_parent}= $state->{_parent};
	$self->{_parent}->next_elem($self) if $self->{_parent};
}

1;
