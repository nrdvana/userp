package TestUserp;
use strict;
use warnings;
use Carp;
use Exporter 'import';
our @EXPORT= qw( run_unittest check_unittest_output unindent );

our $unittest_exe;

sub run_unittest {
	my $test_name= shift;
	unless (defined $unittest_exe) {
		$unittest_exe= $ENV{UNITTEST_ENTRYPOINT} || 'build/unittest';
		$unittest_exe= "./$unittest_exe" unless $unittest_exe =~ m,/,;
		-e $unittest_exe or croak "unittest binary '$unittest_exe' not found";
		-x $unittest_exe or croak "unittest binary '$unittest_exe' is not executable";
	}
	my $out= `$unittest_exe $test_name`;
	main::is( $?, 0, "unittest $test_name exit code" )
		or croak "Unit test $test_name failed to execute";
	return $out;
}

sub check_unittest_output {
	my ($name, $expected)= @_;
	my $actual= run_unittest($name);
	# for readability and ease of diagnostics, split up the $expected and actual output on comment lines
	my @expected_parts= split /^# (.*)\n/m, $expected;
	my @actual_parts=   split /^# (.*)\n/m, $actual;
	# split will leave an empty string at the start of the list
	# if the very first token was a comment, which is the expected usual case.
	# But if it has length, then expand it to an actual piece of the test case.
	if (length $expected_parts[0]) { unshift @expected_parts, 'output' } else { shift @expected_parts }
	if (length $actual_parts[0])   { unshift @actual_parts  , 'output' } else { shift @actual_parts }
	my %actual_by_name= @actual_parts;
	my $success= 1;
	while (my ($name, $output)= splice(@expected_parts, 0, 2)) {
		# output is treated as a regex if it starts with and ends with '/'
		if ($output =~ m,^/(.*?)/$,m) {
			my $regex= '';
			my @parts= split m,^/(.*?)/$,m, $output;
			# even parts are literals, odd parts are regexes
			while (my ($literal, $re)= splice @parts, 0, 2) {
				$regex .= quotemeta($literal) . (defined $re? $re : '');
			}
			$regex =~ s/\\\n/\\n/g; # use \n escape notations instead of a backslash followed by literal newline char
			$output= qr/^$regex$/;
			main::like( delete $actual_by_name{$name}, $output, $name ) or $success= 0;
		} else {
			main::is( delete $actual_by_name{$name}, $output, $name ) or $success= 0;
		}
	}
	if (keys %actual_by_name) {
		main::fail( 'extra unexpected output parts: '. join ', ', keys %actual_by_name );
		$success= 0;
	}
	return $success;
}

# this is basically just a substitute for the Perl 5.26 feature that lets you do indented here-docs
sub unindent {
	my $test= shift;
	my $indent= $test =~ /^( *)/;
	my $n= length $1;
	$test =~ s/^ {$n}//mg;
	return $test;
}

1;
