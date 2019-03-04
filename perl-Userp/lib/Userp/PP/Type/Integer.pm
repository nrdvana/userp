package Userp::PP::Type::Integer;
use Moo;
extends 'Userp::PP::Type';

has min_val => ( is => 'ro' );
has max_val => ( is => 'ro' );

sub _build_discrete_val_count {
	my $self= shift;
	return undef unless defined $self->min_val && defined $self->max_val;
	return $self->max_val - $self->min_val + 1;
}

1;
