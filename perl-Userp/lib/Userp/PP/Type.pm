package Userp::PP::Type;
use Moo;

has ident      => ( is => 'ro', required => 1 );
has spec       => ( is => 'ro', required => 1 );
has align      => ( is => 'rwp' );
has tail_align => ( is => 'rwp' );

1;
