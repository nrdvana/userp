#! /usr/bin/env perl
use FindBin;
use lib $FindBin::Bin;
use strict;
use warnings;
use CTap;
use Getopt::Long;

my $get= GetOptions(
	'o=s'    => \my $dest,
	'inline' => \my $inline,
);
$get && defined $dest or die "Usage: generate_testcase.pl -o DEST SRC [...]\n";

CTap->new(inline => $inline)->include(@ARGV)->write($dest);
