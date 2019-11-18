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
	my ($class, undef, $needed, $for)= @_;
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

sub assert_minmax {
	my ($class, $val, $min, $max, $what)= @_;
	return unless defined $val;
	if (defined $min && $val < $min) {
		$what ||= 'Value';
		$class->throw(
			message => "$what '$val' out of bounds (min $min)",
			value => $val, min => $min
		);
	}
	if (defined $max && $val > $max) {
		$what ||= 'Value';
		$class->throw(
			message => "$what '$val' out of bounds (max $max)",
			value => $val, max => $max
		);
	}
	$val;
}

package Userp::Error::Readonly;
use Moo;
extends 'Userp::Error';

package Userp::Error::NotInScope;
use Moo;
extends 'Userp::Error';

1;
