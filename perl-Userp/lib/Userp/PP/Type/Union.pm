package Userp::PP::Type::Union;
use Moo;
extends 'Userp::PP::Type';

=head1 ATTRIBUTES

=head2 members

An array of types.

=head2 merge

Normally a union is encoded as a selector between its members, and then the value is encoded
according to its specific type.  If this setting is true, any member types which
are fixed-range integer types (and at most one which is an un-bounded integer type) will be
concatenated so that values can be encoded as a single integer of the combined range.
This can save a little space at the cost of encoding/decoding speed.

=cut

has members => ( is => 'ro', required => 1 );
has merge   => ( is => 'ro' );

sub _build_bitsizeof {
	my $self= shift;
	# there is only a bitsizeof if every member merges into a finite integer range,
	# or if they aren't merged and all have the same bitsizeof
	if ($self->merge) {
		my $dv= $self->discrete_val_count;
		return defined $dv? Userp::PP::Type::_bitlen($dv-1) : undef;
	}
	else {
		my $prev;
		for (@{ $self->members }) {
			my $bitlen= $_->bitsizeof;
			return undef unless defined $bitlen && (!defined $prev || $prev == $bitlen);
			$prev= $bitlen;
		}
		# If got here, then every member has the same $bitlen. Add the bits of the selector.
		return $prev + Userp::PP:Type::_bitlen($#{$self->members});
	}
}

sub _build_discrete_val_count {
	my $self= shift;
	my $n= 0;
	for (@{ $self->members }) {
		my $dv= $_->discrete_val_count
			or return undef;
		$n += $dv;
	}
	return $dv;
}

1;
