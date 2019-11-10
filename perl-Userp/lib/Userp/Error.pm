package Userp::Error;
use Moo;
use overload '""' => sub { shift->message };
with 'Throwable';
has message => ( is => 'ro', required => 1 );

package Userp::Error::EOF;
use Moo;
extends 'Userp::Error';
has '+message' => ( default => 'EOF' );
has buffer => ( is => 'rw' );
has pos    => ( is => 'rw' );
has needed => ( is => 'rw' );
has goal   => ( is => 'rw' );

sub for_buf_pos {
	my ($class, undef, $needed, $goal)= @_;
	my $lacking= pos($_[1]) + $needed - length($_[1]);
	$class->new(
		buffer => $_[1], pos => pos($_[1]), needed => $needed, for => $for,
		message => 'Read overrun by '.$lacking.' bytes while reading '.$for
	);
}

package Userp::Error::ImplLimit;
use Moo;
extends 'Userp::Error';
has '+message' => ( default => 'Value exceeds implementation limits' );

package Userp::Error::Domain;
use Moo;
extends 'Userp::Error';
has '+message' => ( default => 'Value of out bounds' );
has value => ( is => 'rw' );
has min   => ( is => 'rw' );
has max   => ( is => 'rw' );

1;
