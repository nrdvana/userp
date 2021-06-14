#! /usr/bin/env perl
use Test::More;
my $test_bin= $ENV{TEST_ENTRYPOINT} || 'build/test';
$test_bin = "./$test_bin" unless $test_bin =~ m,/,;
-e $test_bin or die "unittest binary '$test_bin' not found\n";
-x $test_bin or die "unittest binary '$test_bin' is not executable\n";

# AUTO GENERATED
subtest simple_string => sub {
    my $expected= 
       "Simple string\n"
      ."wrote=13\n"
      ."\n"
      ."wrote=0\n";
    my $actual= `$test_bin simple_string`;
    is( $?, 0, "test exited cleanly" );
    is( $actual, $expected, "output" );
};

# AUTO GENERATED
subtest tpl_ref_static_string => sub {
    my $expected= 
       "String ref 'TEST'\n"
      ."wrote=17\n"
      ."String ref '2', and '1'\n"
      ."wrote=23\n";
    my $actual= `$test_bin tpl_ref_static_string`;
    is( $?, 0, "test exited cleanly" );
    is( $actual, $expected, "output" );
};

done_testing
