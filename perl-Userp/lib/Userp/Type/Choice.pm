package Userp::Type::Choice;
use Moo;
extends 'Userp::Type';

=head1 DESCRIPTION

A Choice is less of a type and more of a protocol mechanism to let you select values from other
types with a space-efficient selector.  The selector is an integer where each value indicates a
value from another type, or indicates a type whose value will follow the selector.

The options of a choice can be:

  * A single value (of any type), occupying a single value of the selector.
  * A finite range of values from the initial integer portion of another type, occupying a
     range of values in the selector.
  * One or more half-infinite ranges from the integer portion of another type, which occupy
     interleaved positions in the selector and cause it to also have a half-infinite domain.
  * An entire type, occupying a single value in the selector and followed by an encoding of
     a value of that type.

The "integer portion" of an Integer is obviously the integer.  The integer portion of a Choice
is the selector.  The integer portion of an array is the element count, unless it has a
constant length in which case it doesn't have one.  The integer portion of a record is either
the integer portion of the first static element, or the count of dynamic elements if there are
no static elements.

=head1 SPECIFICATION

The options of a Choice are given as a list.  Each element is either a type, an option record
(type name "O") which can contain range-merge details, or a typed value.

  (options=(!Type1 !Type2 !O(!Type3 0 20) !Type4:Value4 !Type5))

=head1 ATTRIBUTES

=head2 options

This is an arrayref of the Choice's options.  Each element is a record of the form:

  { type => $type, merge_ofs => $int_min, merge_count => $int_count, value => $value }

C<$type> refers to some other Type in the current scope.

C<$int_min> is optional, and refers to the starting offset for the other type's integer encoding.
C<$int_count> is likewise the number of values taken from the other type's integer encoding.
If neither are given, the option occupies a single value of the selector.  If C<$int_min>
is given but count is not, the entire domain of that type will be included in the selector.

C<$value> is an optional value of that other type which should be represented by this option.
During encoding, that value will be encoded as just the selector value of this option.
During decoding, the reader will return the value as if it had just been decoded.

=head2 align

See L<Userp::PP::Type>

=cut

has options => ( is => 'ro', required => 1 );

has _option_table => ( is => 'lazy' );
sub _build__option_table {
	my $self= shift;
	my $offset= 0;
	my (@table, @inf_opts);
	for my $opt (@{ $self->{options} }) {
		# Is it inlined?
		if (defined $opt->{merge_ofs}) {
			# Is it finite?
			if ($opt->{merge_count} || defined $opt->{type}->scalar_component_max) {
				my $merge_count= $opt->{merge_count} || $opt->{type}->scalar_component_max+1;
				push @table, { %$opt, sel_ofs => $offset, sel_max => $offset+$merge_count-1 };
				$offset += $merge_count;
			}
			# else infinite waits til the end
			else {
				push @inf_opts, $opt;
			}
		}
		else {
			# else it gets a single point on the selector
			push @table, { %$opt, sel_ofs => $offset, sel_max => $offset };
			++$offset;
		}
	}
	# Infinite options come last, selected by low bits
	if (@inf_opts) {
		my $sel_bits= _bitlen($#inf_opts);
		my $i= 0;
		push @table, map +{ %$_, sel_ofs => $offset, sel_shift => $sel_bits, sel_flag => $i++ }, @inf_opts;
	}
	return \@table;
}

has _option_tree => ( is => 'lazy' );
sub _build__option_tree {
	my $self= shift;
	my $root= {};
	for (@{ $self->_option_table }) {
		my $type= $_->{type};
		# If user asks for $type, this is one of the options.
		my $by_type= ($root->{$type->id} ||= {});
		my $generic= $type->isa_int? 'int'
			: $type->isa_sym? 'sym'
			: $type->isa_typeref? 'typ'
			: $type->isa_array || $type->isa_record? 'seq'
			: undef;
		my $by_generic= $generic? ($root->{$generic} ||= {}) : undef;
		# If this is a value, add to list of values
		if (defined $_->{value}) {
			$by_type->{values}{$_->{value}} ||= $_;
			$by_generic->{values}{$_->{value}} ||= $_ if defined $by_generic;
		}
		# If it is a sub-range of the type, add it to ranges
		elsif (defined $_->{merge_count}) {
			push @{$by_type->{ranges}}, $_;
			push @{$by_generic->{ranges}}, $_ if defined $by_generic;
		}
		# If it is the first reference to the whole type, store as '.'
		else {
			$by_type->{'.'} ||= $_;
			$by_generic->{'.'} ||= $_ if defined $by_generic;
		}
	}
	# TODO: for each option which is a Choice type, merge all those options into this node.
	$root;
}

sub isa_Choice { 1 }

sub has_scalar_component { 1 }

has scalar_component_max => ( is => 'lazy' );
sub _build_scalar_component_max { shift->option_table->[-1]{sel_max} }

sub _register_symbols {
	my ($self, $scope)= @_;
	for (@{ $self->options }) {
		die "Unimplemented: register symbols found in option->value"
			if defined $_->{value};
	}
	$self->next::method($scope);
}

1;
