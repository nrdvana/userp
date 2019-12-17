package Userp::Error;
use Moo;
use overload '""' => sub { shift->message };
with 'Throwable';
has message => ( is => 'ro', required => 1 );

package Userp::Error::EOF;
use Moo;
extends 'Userp::Error';
has '+message' => ( lazy => 1, builder => 1, required => 0 );
has buffer_ref => ( is => 'rw' );
has bitpos     => ( is => 'rw' );
has needed     => ( is => 'rw' );
has goal       => ( is => 'rw' );

sub _build_message {
	my $self= shift;
	my $msg= 'EOF';
	$msg .= ' while reading '.$self->goal if defined $self->goal;
	if ($self->needed) {
		$msg .= defined $self->goal? ' of ' : ' while reading ';
		$msg .= $self->needed & 8? $self->needed.' bits' : ($self->needed>>3).' bytes';
		if (defined $self->buffer_ref && defined $self->bitpos) {
			my $avail= (length(${$self->buffer_ref})<<3) - $self->bitpos;
			$msg .= ' from buffer with '.($avail & 8? $avail.' bits' : ($avail>>3).' bytes').' remaining';
		}
	}
	$msg;
}

sub for_buf {
	my ($class, undef, $bytepos, $needed, $goal)= @_;
	$class->new(buffer_ref => \$_[1], bitpos => $bytepos<<3, needed => $needed<<3, goal => $goal);
}
sub for_buf_bits {
	my ($class, undef, $bitpos, $needed, $goal)= @_;
	$class->new(buffer_ref => \$_[1], bitpos => $bitpos, needed => $needed, goal => $goal);
}

package Userp::Error::System;
use Moo;
extends 'Userp::Error';
has '+message' => ( lazy => 1, default => sub { my $self= shift; join ': ', grep defined, $self->syscall, $self->errstr } );
has errno  => ( is => 'rw', default => sub { $!+0 } );
has errstr => ( is => 'rw', default => sub { $! } );
has errset => ( is => 'rw', default => sub { +{ map { $!{$_}? ($_ => $!{$_}) : () } keys %! } } );

package Userp::Error::ImplLimit;
use Moo;
extends 'Userp::Error';
has '+message' => ( default => 'Value exceeds implementation limits' );

package Userp::Error::Domain;
use Moo;
extends 'Userp::Error';
has '+message' => ( lazy => 1, builder => 1, required => 0 );
has valtype => ( is => 'rw' );
has value   => ( is => 'rw' );
has min     => ( is => 'rw' );
has max     => ( is => 'rw' );

sub _build_message {
	my $self= shift;
	my $what= $self->valtype || 'Value';
	return $what.' undefined' unless defined $self->value;
	return $what.' "'.$self->value.'" greater than maximum bound "'.$self->max.'"'
		if defined $self->max && $self->value > $self->max;
	return $what.' "'.$self->value.'" less than minimum bound "'.$self->min.'"'
		if defined $self->min && $self->value < $self->min;
	return $what.' "'.$self->value.'" out of bounds';
}

sub new_minmax {
	my ($class, $val, $min, $max, $what)= @_;
	$class->new(value => $val, min => $min, max => $max, valtype => $what);
}

sub assert_minmax {
	my ($class, $val, $min, $max, $what)= @_;
	return $val if defined $val && (!defined $min || $val >= $min) && (!defined $max || $val <= $max);
	$class->new_minmax($val, $min, $max, $what)->throw;
}

package Userp::Error::API;
use Moo;
extends 'Userp::Error';

package Userp::Error::Readonly;
use Moo;
extends 'Userp::Error';

package Userp::Error::NotInScope;
use Moo;
extends 'Userp::Error';

package Userp::Error::Protocol;
use Moo;
extends 'Userp::Error';

1;
