package Userp::PP::Type::Integer;
use Moo;
extends 'Userp::PP::Type';

has endian  => ( is => 'ro' );
has min_val => ( is => 'ro' );
has max_val => ( is => 'ro' );

1;
