package Userp::Type::Choice;
use Carp;
use Moo;
extends 'Userp::Type';

=head1 SYNOPSIS

  # An enumeration of symbols ( See also: Integer->names )
  $scope->define_type('Choice', 'ErrorCodes',
    options => [
      { type => $scope->type_Symbol, value => 'EWOULDBLOCK' },
      { type => $scope->type_Symbol, value => 'EBADF' },
      { type => $scope->type_Symbol, value => 'EAGAIN' },
      { type => $scope->type_Symbol, value => 'EINVAL' },
    ]
  );
  
  # A hybrid type
  $scope->define_type('Choice', 'FailureReason',
    options => [
      { type => $errorcode_t },
      { type => $scope->type_String },
    ]
  );
  
  # A restriction of types
  $scope->define_type('Choice', 'Int_Or_Str',
    options => [
      { type => $scope->type_Integer },
      { type => $scope->type_String },
    ],
    # Hint to decoder that this type is not an official detail of the data
    transparent => 1
  );
  
  # Optimizing common cases
  $scope->define_type('Choice', 'Optimal_Small_Int_Or_Str',
    options => [
      # numbers -100 to 100
      { type => $scope->type_Integer, merge_ofs => 0, merge_count => 201 },
    
      # strings of length 0 to 50
      { type => $scope->type_String, merge_ofs => 0, merge_count => 50 },
    
      # or anything else
      { type => $scope->type_Any }
    ],
    # Hint to decoder that this type is not an official detail of the data
    transparent => 1
  );

=head1 DESCRIPTION

A Choice can be used as an Enumeration of values, a hybrid type of other types, a way to
constrain the possible types or values allowed, or just as a protocol mechanism to efficiently
encode the most common values.  Sometimes a Choice is an official classification of the data,
but other times it's just an encoding detail.  Use the C<transparent> metadata flag to tell the
reader which you intend.

A choice is encoded as a single integer "selector", which indicates an option.  If the option
is a type (not a value), the selector is followed by an encoded value of that type.

To make the most use of the selector, pieces of the following value may be merged into some
range of integers in the selector.  For instance, C<0> might indicate an Integer follows, but
the values C<1..10> could indicate integer values C<0..9> removing the need to encode those
integers as a second byte.  See L</MERGING OPTION VALUES> for a in-depth discussion.

=head1 ATTRIBUTES

=head2 name

See L<Userp::Type/name>

=head2 parent

A choice type which this type inherits options from.  Options passed to the constructor will be
appended to those of the parent.  There is no way to re-define the inherited options.

=head2 options

This is an arrayref of the Choice's options.  Each element is a record of the form:

  { type => ..., value => ... }

or

  { type => $type, merge_ofs => $int, merge_count => $int, merge_only => $bool }

C<type> refers to some other Type in the current scope.

C<merge_ofs>, C<merge_count>, C<merge_only> are encoding optimizations to make use of unused
bits in the Choice's selector.  See L</MERGING OPTION VALUES>.

For ease of use, the list you pass to the constructor may contain strings (which coerce to
C<< { type => 'Userp.Symbol', value => $str } >>) or bare types objects (which coerce to
C<< { type => $type } >>), or an arrayref of C<< [ $type, $value ] >>.

=head2 transparent

A boolean flag indicating whether the decoder should "see" this type, passing it on to the
user, or pass over this type and show the user the selected option's type.

=head2 align

Specify the starting alignment needed by the Choice's selector.  If the choice has infinite
ranges merged into it, C<align> cannot be less than 0.

=head2 metadata

See L<Userp::Type/metadata>.

=head1 METHODS

=head2 new

The constructor takes any of the above attributes.  C<options> is required unless it exists in
parent.

=head2 subtype

L<Userp::Type/subtype>

=head2 get_definition_attributes

L<Userp::Type/get_definition_attributes>

=head1 MERGING OPTION VALUES

Consider for example a type with two choices:

  options => [
    { type => 'Userp.Symbol', value => "N/A" },
    { type => 'Userp.IntU' }
  ]

This gets encoded as a selector of 0 (for the symbol) or 1 (for the integer).  In the first
case, the value is complete, but in the second case it is followed by a byte-aligned
variable-length integer value.  Encoding the integer 3 would then require 2 bytes.

To mke better use of those bits, you can merge all or part of the first integer of a option's
type into the selector itself.

  options => [
    { type= > 'Userp.Symbol', value => "N/A" },
	{ type => 'Userp.Integer', merge_ofs => 0 }
  ]

In this case, the encoded selector contains the entire second option's value, and is logically
similar to the following code:

  my $val= $buffer->decode_int(); # variable-length integer
  if ($val == 0) { return "N/A" }
  else { return $val - 1 }

Another option is to only merge a portion of the next integer:

  options => [
    { type => 'Userp.Symbol', value => "N/A" },
	{ type => 'Userp.Integer', merge_ofs => 2, merge_count => 254 }
  ]

This merges only the values 2 through 255 into the selector, resulting in a single-byte
selector where all 256 values mean something:

  my $val= $buffer->decode_int(8); # 8-bit selector
  if ($val == 0) { return "N/A" }
  elsif ($val == 1) { return $buffer->decode_int }
  else { return $val } # values 2..255 can be returned as-is

And finally, if for some reason you want to constrain the value of the type to only the ones
merged into the selector, you can add C<merge_only>:

  options => [
    { type => 'Userp.Symbol', value => "N/A" },
	{ type => 'Userp.Integer', merge_ofs => 0, merge_count => 255, merge_only => 1 }
  ]

resulting in code logically similar to:

  my $val= $buffer->decode_int(8); # 8-bit selector
  if ($val == 0) { return "N/A" }
  else { return $val-1 } # values 0..254

During encoding, it will throw an error if you try to write a value below zero or higher than 254.

For a more interesting example, consider a record with optional fields:

  my $layout_t= Userp::Type::Record->new(fields => [
	[ bgcolor     => $color_t, OFTEN ],
	[ color       => $color_t, OFTEN ],
	[ font        => 'Userp.String', OFTEN ],
	[ line_height => 'Userp.Float32', OFTEN ],
	[ line_indent => 'Userp.Float32', OFTEN ],
	[ margin      => 'Userp.Float32', OFTEN ],
	[ padding     => 'Userp.Float32', OFTEN ],
	[ border      => $border_t, OFTEN ],
  ]);

There are 8 optional fields with placement declared as "OFTEN", so the record will begin with
8 bits indicating whether the field is present.  Part of that can be merged into a choice
selector:

  my $album_or_null= Userp::Type::Choice(options => [
	{ type => 'Userp.Symbol', value => 'N/A' },
	{ type => 'Userp.Symbol', value => 'Inherit' },
	{ type => 'Userp.Symbol', value => 'Default' },
	{ type => $album_t, merge_ofs => 1, merge_count => 252 }
  ]);

This choice combines three constants with most of the possible combinations of optional fields
for the record into a single byte, while not limiting which record fields combinations can be
written.

  # Selector Value     Meaning
  0                    Symbol 'N/A'
  1                    Symbol 'Inherit'
  2                    Symbol 'Default'
  3                    Record "Layout", entire encoding follows this selector
  4                    Record "Layout" with field 'bgcolor'
  5                    Record "Layout" with field 'color'
  6                    Record "Layout" with fields 'bgcolor','color'
  ...


=cut

has options     => ( is => 'ro', required => 1, coerce => \&_coerce_option_list );
has transparent => ( is => 'ro' );

my %option_keys= ( type => 1, value => 1, merge_count => 1, merge_ofs => 1, merge_only => 1 );
sub _coerce_option_list {
	my @opts= @{ $_[0] };
	for (@opts) {
		if (!ref $_) {
			my $t= Userp::Bits::is_valid_symbol($_)? 'Userp.Symbol'
				: 'Userp.String';
			$_ = { type => $t, value => $_ };
		}
		elsif (ref $_ eq 'ARRAY') {
			$_ = { type => $_->[0], value => $_->[1] }
		}
		elsif (ref $_ eq 'HASH') {
			defined $_->{type} or croak "option 'type' is a required field";
			if (exists $_->{value}) {
				croak "key '$_' is not valid for option with value"
					for grep { $_ ne 'type' && $_ ne 'value' } keys %$_;
			}
			else {
				croak "key '$_' is not valid for option"
					for grep !$option_keys{$_}, keys %$_;
			}
		}
		else {
			croak "Unknown specification for option: '$_'"
		}
	}
	Const::Fast::const my $ret => \@opts;
	return $ret;
}

#has _option_table => ( is => 'lazy' );
#sub _build__option_table {
#	my $self= shift;
#	my $offset= 0;
#	my (@table, @inf_opts);
#	for my $opt (@{ $self->{options} }) {
#		# Is it inlined?
#		if (defined $opt->{merge_ofs}) {
#			# Is it finite?
#			if ($opt->{merge_count} || defined $opt->{type}->scalar_component_max) {
#				my $merge_count= $opt->{merge_count} || $opt->{type}->scalar_component_max+1;
#				push @table, { %$opt, sel_ofs => $offset, sel_max => $offset+$merge_count-1 };
#				$offset += $merge_count;
#			}
#			# else infinite waits til the end
#			else {
#				push @inf_opts, $opt;
#			}
#		}
#		else {
#			# else it gets a single point on the selector
#			push @table, { %$opt, sel_ofs => $offset, sel_max => $offset };
#			++$offset;
#		}
#	}
#	# Infinite options come last, selected by low bits
#	if (@inf_opts) {
#		my $sel_bits= _bitlen($#inf_opts);
#		my $i= 0;
#		push @table, map +{ %$_, sel_ofs => $offset, sel_shift => $sel_bits, sel_flag => $i++ }, @inf_opts;
#	}
#	return \@table;
#}
#
#has _option_tree => ( is => 'lazy' );
#sub _build__option_tree {
#	my $self= shift;
#	my $root= {};
#	for (@{ $self->_option_table }) {
#		my $type= $_->{type};
#		# If user asks for $type, this is one of the options.
#		my $by_type= ($root->{$type->id} ||= {});
#		my $generic= $type->isa_int? 'int'
#			: $type->isa_sym? 'sym'
#			: $type->isa_typeref? 'typ'
#			: $type->isa_array || $type->isa_record? 'seq'
#			: undef;
#		my $by_generic= $generic? ($root->{$generic} ||= {}) : undef;
#		# If this is a value, add to list of values
#		if (defined $_->{value}) {
#			$by_type->{values}{$_->{value}} ||= $_;
#			$by_generic->{values}{$_->{value}} ||= $_ if defined $by_generic;
#		}
#		# If it is a sub-range of the type, add it to ranges
#		elsif (defined $_->{merge_count}) {
#			push @{$by_type->{ranges}}, $_;
#			push @{$by_generic->{ranges}}, $_ if defined $by_generic;
#		}
#		# If it is the first reference to the whole type, store as '.'
#		else {
#			$by_type->{'.'} ||= $_;
#			$by_generic->{'.'} ||= $_ if defined $by_generic;
#		}
#	}
#	# TODO: for each option which is a Choice type, merge all those options into this node.
#	$root;
#}

sub isa_Choice { 1 }
sub base_type { 'Choice' }

sub _get_definition_attributes {
	my ($self, $attrs)= @_;
	if (my $parent= $self->parent) {
		$attrs->{options}= [ @{$self->options}[ 0 .. $#{$self->parent->options} ] ];
		$attrs->{transparent}= ($self->transparent? 1:0)
			if ($self->transparent? 1:0) != ($self->parent->transparent? 1:0);
	} else {
		$attrs->{options}= $self->options;
		$attrs->{transparent}= 1 if $self->transparent;
	}
	$attrs;
}

sub _find_symbols_in_record {
	my ($scope, $value)= @_;
	if (ref $value eq 'HASH') {
		$scope->register_symbols(keys %$value);
		ref $_ && _find_symbols_in_record($scope, $_) for values %$value;
	}
	elsif (ref $value eq 'ARRAY') {
		ref $_ && _find_symbols_in_record($scope, $_) for @$value;
	}
}

sub _register_symbols {
	my ($self, $scope)= @_;
	for (@{ $self->options }) {
		if ($_->{type}->isa_Symbol && defined $_->{value}) {
			$scope->add_symbols($_->{value});
		}
		elsif ($_->{type}->isa_Record && defined $_->{value}) {
			_find_symbols_in_record($scope, $_->{value})
		}
	}
	$self->next::method($scope);
}

1;
