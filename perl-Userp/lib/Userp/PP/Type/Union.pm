package Userp::PP::Type::Union;
use Moo;
extends 'Userp::PP::Type';

=head1 DESCRIPTION

If you consider a type to be a "set of values", a Union is a "set of sets".  Its purpose in the
protocol is to constrain the possible types that can appear in order to make encoding more
efficient (or maybe to help an application validate it's data).

=head1 ATTRIBUTES

=head2 members

An array of types.

=head2 merge

Normally a union is encoded as a selector between its member types, and then the value is
encoded according to its specific type.  If this setting is true, the first "scalar component"
of the encoding of each member type will be merged into a single unsigned integer.  This can
save a little space at the cost of encoding/decoding speed.  The main purpose of this feature
is to "steal" a few values or bits from a big type to indicate some other condition like "NULL".
For example, you could encode a type that is Null, True, False, or Integer and the encoding
would be 0, 1, 2, or the integer + 3.

Note that a Union of Unions always has a "merge" effect as if the members of the inner union
has been added to the member list of the parent.  They do *not* get de-duplicated, however,
because a type might contain special metadata and the reader might want to know the full "path"
of union inclusion.

=cut

has members => ( is => 'ro', required => 1 );
has merge   => ( is => 'ro' );

has union_member_table => ( is => 'lazy' );
sub _build_union_member_table {
	my $self= shift;
	my @table;
	my @inf_members;
	if (!$self->merge) {
		for (@{$self->members}) {
			if ($_->can('union_member_table') && !$_->merge) {
				push @table, @{ $_->union_member_table };
			} else {
				push @table, $_;
			}
		}
	}
	else {
		my $offset= 0;
		my @todo= @{$self->members};
		while (shift @todo) {
			# Merging another union consumes that union's members as if they were part of this list.
			if ($_->can('union_member_table')) {
				if ($_->merge) {
					my @items= map [ @$_ ], $_->union_member_table; # shallow clone
					# if final element is inf_members, move them into that list
					if (!$items[-1][1]) {
						push @inf_members, @{ $items[-1][2] };
						pop @items;
					}
					$_->[0] += $offset for @items; # increase offset of each
					push @table, $items;
					$offset= $items[-1][0] + $items[-1][1];
				} else {
					unshift @todo, @{$_->members};
				}
			}
			# Merging a Sequence only happens if the sequence is variable-length (and not null-terminated)
			elsif ($_->can('len_type') && $_->len_type) {
				my $n= $_->len_type->distinct_val_count;
				if ($n) {
					push @table, [ $offset, $n, $_ ];
					$offset += $n;
				} else {
					push @inf_members, $_;
				}
			}
			# Merging an enum only happens if it is not "encode_literal"
			# This case also handles merging integer types.
			elsif (!$_->can('encode_literal') || !$_->encode_literal) {
				my $n= $_->distinct_val_count;
				if ($n) {
					push @table, [ $offset, $n, $_ ];
					$offset += $n;
				} else {
					push @inf_members, $_;
				}
			}
			# Else no merge possible.  Encode as a selector followed by the type itself.
			else {
				push @table, [ $offset, 0, $_ ];
				$offset++;
			}
		}
		# Final element is inf-members, if any.
		push @table, [ $offset, 0, \@inf_members ]
			if @inf_members;
	}
}

sub _build_discrete_val_count {
	my $self= shift;
	my $table= $self->union_member_table;
	# The union is a single integer if it is fully merged, or if every member type is a constant.
	if ($self->merged) {
		return $table->[-1][1] == 0? undef : $table->[-1][0] + $table->[-1][1];
	}
	else {
		unless $_->discrete_val_count == 1 or return undef
			for @$table;
		return scalar @$table;
	}
}

sub _build_bitsizeof {
	my ($self, $bytes)= @_;
	my $table= $self->union_member_table;
	# The union is a single integer if it is fully merged, or if every member type is a constant.
	if ($self->merged) {
		return undef if $table->[-1][1] == 0;
		my $bits= Userp::PP::Type::_bitlen($table->[-1][0] + $table->[-1][1] - 1);
		return $bytes? ($bits+7)>>3 : $bits;
	}
	else {
		my $size;
		for (@$table) {
			my $memb_size= $_->sizeof : $_->bitsizeof;
			$size= $memb_size unless defined $size;
			return undef unless defined $memb_size and $size == $memb_size;
		}
		my $sel_bits= Userp::PP::Type::_bitlen($#$table);
		return $size + ($bytes? ($sel_bits+7)>>3 : $sel_bits);
	}
}

sub _build_sizeof {
	shift->_build_bitsizof(1); # shares code with above
}

1;
