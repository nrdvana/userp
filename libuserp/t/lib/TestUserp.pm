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
		$unittest_exe= $ENV{UNITTEST_ENTRYPOINT}
			|| -f 'build/unittest'? 'build/unittest'
			 : -f './unittest'? './unittest'
			 : '../unittest';
		$unittest_exe= "./$unittest_exe" unless $unittest_exe =~ m,/,;
		-e $unittest_exe or croak "unittest binary '$unittest_exe' not found";
		-x $unittest_exe or croak "unittest binary '$unittest_exe' is not executable";
	}
	my $out= `$unittest_exe $test_name 2>&1`;
	unless (main::is( $?, 0, "unittest $test_name exit code" )) {
		main::diag $out;
		croak "Unit test $test_name failed to execute";
	}
	return $out;
}

sub check_unittest_output {
	my ($name, $expected)= @_;
	my $actual= run_unittest($name);

	pos($expected)= 0;
	pos($actual)= 0;
	my $resync= 0;
	my $success= 1;
	while (pos $expected < length $expected) {
		# read next line of $expected, excluding the newline chars
		$expected =~ /\G([^\r\n]*)\r?(\n|$)/gc or die substr($expected, pos $expected);
		my $line= $1;
		# If it's a regex notation, /.../, use as-is, else convert the literal into a regex.
		my $re_text= ($line =~ m,^/(.*)/$,)? $1 : quotemeta($line);
		$re_text =~ s/\\\n/\\n/g; # use \n escape notations instead of a backslash followed by literal newline char
		$re_text= '\G'.$re_text unless $resync;
		# Match against the next portion of $actual
		if ($actual =~ /$re_text/gc) {
			# throw away line ending
			$actual =~ /\G[\r\n]*/gc;
			main::pass("Found output: $line");
			$resync= 0;
		} else {
			# mismatch.  Report the error, then stop anchoring until the next successful match.
			main::fail("Found output: $line");
			$success= 0;
			# cut off the output at the following comment, if any.
			$actual =~ /\G(.[^#]+)/;
			main::diag("Output does not match /$re_text/ : ".output_text_to_perl($1));
			$resync= 1;
		}
	}
	if (pos $actual == length $actual) {
		main::pass("Found expected output end");
	} else {
		main::fail("Unexpected extra output");
		main::diag("extra output:".output_text_to_perl(substr($actual, pos $actual)));
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

our %escapes= (
	"\n" => '\\n',
	"\t" => '\\t',
	'$' => '\\$',
	'@' => '\\@',
);
sub output_text_to_perl {
	my $string= shift;
	sub esc {
		defined $escapes{$_}? $escapes{$_} : sprintf("\\x%02X", ord)
	}
	return ' '.join "\n.", map { s/[\$\@\"\\\x00-\x1F\x7F-\xFF]/esc/eg; qq{"$_\\n"} }
		split /\n/, $string;
}

1;
