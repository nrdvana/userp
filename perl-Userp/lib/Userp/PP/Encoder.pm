package Userp::PP::Encoder;

=head1 DESCRIPTION

This class is the API for serializing data into the types defined in the current scope.

The sequence of API calls depends on type of the value being encoded.  Here is a brief
overview:

=over

=item Integer

  $enc->int($int_value);

=item Enum

  $enc->ident($identifier);   # string identifier or Ident object
  $enc->int($int_value);      # for any enum where encode_literal is true

=item Union

  $enc->type($some_type)->...  # select a member (or sub-member) type
  
In some cases, you skip straight to encoding the value, if the encoder can figure out which
sub-type is required.

=item Sequence

  $enc->begin($length); # length may or may not be required
  for(...) {
    $enc->elem($identifier); # needed if elements are named and ident is not pre-defined
    $enc->elem($index);      # needed if array length is "sparse"
    $enc->...(...)
  }
  $enc->end();
  
  # Typical variable-length array of type Any:
  # ( equivalent to pack('VV', 1, 2) )
  $enc->begin(2)->type(UInt32)->int(1)->type(UInt32)->int(2)->end;
  
  # Typical fixed-length record with named fields:
  $enc->begin->elem('x')->float(1)->elem('y')->float(2)->end;
    
  # Special case for byte/integer arrays or strings or anything compatible:
  $enc->encode_array(\@values);
    
  # Special case for named element sequences
  $enc->encode_record(\%key_val);

=back

=head1 ATTRIBUTES

=head2 scope

The current L<Userp::PP::Scope> which defines what types and identifiers are available.

=head2 bigendian

True for ecoding with largest-byte-first, false for encoding with smallest-byte-first.

=head2 node_type

The type expected for the current node.  If this is a union (including TypeAny) you can select
sub-types with method L</type>.

=cut

has scope        => ( is => 'ro', required => 1 );
has bigendian    => ( is => 'ro', required => 1 );

has current_type => ( is => 'rwp' );
has _choice_path => ( is => 'rw', default => sub { [] } );

=head1 METHODS

=head2 type

  $enc->type($type)->...

Select a sub-type of the current union type.  This includes selecting any type at all when the
current type is TypeAny.  This may be given multiple times to express specific members of
unions, though doing so is not always required if the union-member can be inferred.

=cut

*type= *select_subtype;
sub select_subtype {
	my ($self, $type)= @_;
	$self->current_type->can('options')
		or croak "Cannot select sub-type of ".$self->current_type->name;
	$self->current_type->has_option_of_type($type)
		or croak "Type ".$type->name." is not a valid option for ".$self->current_type->name;
	push @{ $self->_choice_path }, $self->current_type;
	$self->_set_current_type($type);
	$self;
}

sub _encode_scalar_component {
	my ($self, $scalar_value)= @_;
	my @to_encode;
	my $type= $self->current_type;
	my $val= $scalar_value;
	my $choice_path= $self->_choice_path;
	if (@$choice_path) {
		for (reverse @$choice_path) {
			defined (my $opt= $_->_option_for($type, $val))
				or croak "No option of ".$_->name." allows value of (".$type->name." : ".$val.")";
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

=head2 encode_int

  $enc->int($int_value)->...

The C<$int_value> must be an integer (or BigInt) within the domain of the L</current_type>.
Method dies if arguments are invalid or if a different type is required at this point of the
stream.  Returns the encoder for convenient chaining.

=cut

sub int {
	my ($self, $val)= @_;
	my $type= $self->current_type;
	my @type_sel= @{ $self->_type_sel };
	# If an enum, look up the value's index within the member list
	if ($type->can('find_member_by_value')) {
		if (!$type->encode_literal) {
			defined(my $idx= $type->find_member_by_value(int => $val))
				or croak "Enumeration ".$type->name." does not contain value ".$val;
			$self->encode_qty($type->bitsizeof, $val);
			return $self;
		}
		# else fall through with enum's value_type
		$type= $type->value_type;
	}
	# If type is a union, be helpful and search for first union member which is conceptually an
	# integer, and capable of holding this integer value.
	if ($type->can('find_member_containing_int')) {
		my $leaf_type= $type->find_member_containing(int => $val)
			or croak "Integer '$val' is not a member of any type in union ".$type->name;
		$self->encode_union_selector($type, $leaf_type);
		$leaf_type= $leaf_type->value_type;
			if $leaf_type->can('encode_literal') && $leaf_type->encode_literal;
		$self->_set_current_type($type= $leaf_type);
	}
	# Type should be integer, here.
	$type->can('min_val')
		or croak "Type ".$type->name." cannot encode integers";
	# validate min/max
	defined $min || defined $max or croak "Invalid Integer type";
	my $min= $int_type->min_val;
	my $max= $int_type->max_val;
	defined $min and $val < $min and croak "Integer out of bounds for type ".$type->name.": $val < $min";
	defined $max and $val > $max and croak "Integer out of bounds for type ".$type->name.": $val > $max";

	!defined $min? $self->encode_vqty($max - $val)
	: !defined $max? $self->encode_vqty($val - $min)
	: $self->encode_qty($int_type->bitsizeof, $val);
}

=head2 ident

  $enc->ident($dentifier); # Ident object or plain string

=cut

sub ident {
}

=head2 begin

Begin encoding the elements of a sequence.

=head2 end

Finish encoding the elements of a sequence, returning to the parent type, if any.

=cut

sub begin {
	my ($self, $len) @_;
}

sub end {
}

1;
