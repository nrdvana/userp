package Userp::PP::Type::Choice;
use Moo;
with 'Userp::PP::Type';

=head1 DESCRIPTION

A Choice is less of a type and more of a protocol mechanism to let you select values from other
types with a space-efficient selector.  The selector is an integer where each value indicates a
value from another type, or indicates a type whose value will follow the selector.

The options of a choice can be:

  * A single value (of any type), occupying a single value of the selector.
  * A finite range of values from the integer portion of another type, occupying a range of
     values in the selector.
  * One or more half-infinite ranges from the integer portion of another type, which occupy
     interleaved positions in the selector and cause it to also have a half-infinite domain.
  * An entire type, occupying a single value in the selector and followed by an encoding of
     a value of that type.

The "integer portion" of an Integer is obviously the integer.  The integer portion of a Choice
is the selector.  The integer portion of a sequence is the element count, assuming the sequence
was defined to have one and it isn't a constant.

=head1 ATTRIBUTES

=head2 options

This is an arrayref of the Choice's options.  Each element is of the form:

  { type => $type, merge_ofs => $int_min, merge_count => $int_count, value => $value }

Type refers to some other Type in the current scope.

C<int_min> is optional, and refers to the starting offset for the other type's integer encoding.
C<int_count> is likewise the number of values taken from the other type's integer encoding.
If neither are given, the option occupies a single value of the selector.  If C<int_min>
is given but count is not, the entire domain of that type will be included in the selector.

C<$value> is an optional value of that other type which should be represented by this option.
During encoding, that value will be encoded as just the selector value of this option.
During decoding, the reader will return the value as if it had just been decoded.

=cut

has options => ( is => 'ro', required => 1 );

has _option_table => ( is => 'lazy' );
sub _build__option_table {
	my $self= shift;
	my @todo= $self->options;
	my $offset;
	my @table;
	my @inf_opts;
	while (my $opt= shift @todo) {
		# Is it inlined?
		if (defined $opt->{merge_ofs}) {
			# Is it finite?
			my $merge_count= $opt->{merge_count} || $opt->{type}->scalar_component_max;
			if ($merge_count) {
				$offset += $opt->{merge_count};
				push @table, [ $offset, $opt ];
				next;
			}
			# else infinite waits til the end
			elsif (!defined $merge_count) {
				push @inf_opts, $opt;
				next;
			}
		}
		# else it gets a single point on the selector
		push @table, [ ++$offset, $opt ];
	}
	if (@inf_opts) {
		push @table, [ undef, \@inf_opts ];
	}
	return \@table;
}

sub has_scalar_component { 1 }
has scalar_component_max => ( is => 'lazy' );
sub _build_scalar_component_max { shift->option_table->[-1][0] }

1;
