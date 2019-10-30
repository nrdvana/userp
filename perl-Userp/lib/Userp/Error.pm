package Userp::Error;
use Moo;
use overload '""' => sub { shift->message };
with 'Throwable';
has message => ( is => 'ro', required => 1 );

package Userp::Error::EOF;
use Moo;
extends 'Userp::Error';
has '+message' => ( default => 'EOF' );

1;
