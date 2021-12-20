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
			|| (-f 'build/unittest' && 'build/unittest')
			|| (-f './unittest' && './unittest')
			|| '../unittest';
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
	# convert expected to regexes, one per line
	my @regexes= split /\n/, $expected;
	# find the diff
	my $diff= diff_regex_vs_text(\@regexes, $actual);
	#use DDP;main::diag(&np($diff));
	# did it match?
	my $match;
	for ($match= 0; $match < @$diff; $match++) {
		last unless defined $diff->[$match][0] && defined $diff->[$match][1];
	}
	my $ok= $match == @$diff;
	if (main::ok( $ok, "Found expected output" )) {
		main::note(render_diff($diff));
	} else {
		main::diag(render_diff([ @{$diff}[ ($match?$match-1:$match) .. $#$diff ] ]));
	}
	return $ok;
}

sub diff_regex_vs_text {
	my ($regexes, $text, $resync)= @_;
	#use DDP; &p(\@_);
	my @output;
	pos($text)= 0;
	for (my $re_i= 0; $re_i < @$regexes; $re_i++) {
		#print "re_i=$re_i  test $regexes->[$re_i]  resync=".($resync||0)."\n";
		my $t_pos= pos $text;
		my $re= $regexes->[$re_i];
		$re .= '$' if !length $re;
		my $match= $resync? ($text =~ /^$re/gcm)
			: ($text =~ /\G$re/gcm);
		# If it matched, add both regex and matched text
		if ($match) {
			# If resyncing, add any skipped lines
			if ($resync && $-[0] > $t_pos) {
				push @output, [ undef, grep length, split /(.*\n)/, substr($text, $t_pos, $-[0] - $t_pos) ];
				$resync= 0;
			}
			my $matchtext= substr($text, $-[0], $+[0] - $-[0]);
			#&p([ $matchtext, length $matchtext? split(/\n/, $matchtext, -1) : (""), substr($text, pos $text) ])
			#	if !$regexes->[$re_i];
			push @output, [ $regexes->[$re_i], length $matchtext? (grep length, split /(.*\n)/, $matchtext) : ('') ];
			# throw away line ending
			$text =~ /\G\r?\n?/gc;
		}
		else {
			return if $resync; # failed resync match means go back and try next regex
			# Terrible algorithm, but implementing the industry standard diff just for
			#  unit tests seems like overkill.
			# Recursively test every regex from here to the end in 'resync' mode
			# and see which results in the fewest mismatches.
			my ($best, $mismatches);
			for (my $i= $re_i; $i < @$regexes; $i++) {
				my $attempt= diff_regex_vs_text([@{$regexes}[ $i .. $#$regexes ]], substr($text, pos($text)), 1)
					or next;
				my $attempt_mismatches= $i - $re_i;
				for (@$attempt) {
					$attempt_mismatches++ if !defined $_->[0] || !defined $_->[1];
				}
				if (!$best || $attempt_mismatches < $mismatches) {
					$best= [ (map [$_, undef], @{$regexes}[ $re_i .. $i-1 ]), @$attempt ];
					$mismatches= $attempt_mismatches;
				}
			}
			if ($best) {
				push @output, @$best;
			} else {
				push @output, map [ $_, undef ], @{$regexes}[ $re_i .. $#$regexes ];
				push @output, map [ undef, $_ ], grep length, split /(.*\n)/, substr($text, pos($text));
			}
			return \@output;
		}
	}
	# put leftovers in a non-match
	if (pos $text < length $text) {
		push @output, [ undef, split /\n/, substr($text, pos($text)), -1 ];
	}
	return \@output;
}

sub render_diff {
	my $diff= shift;
	my $widest_left= 10;
	for (@$diff) {
		$widest_left= length $_->[0]
			if $_->[0] && length $_->[0] > $widest_left;
	}
	my $out= sprintf("-%s Regex %s-+-%s Output %s\n",
		'-'x(($widest_left-7)/2), '-'x($widest_left - int(($widest_left-7)/2) - 7),
		'-'x(($widest_left-6)/2), '-'x($widest_left - int(($widest_left-6)/2) - 6),
	);
	for (@$diff) {
		my ($re, @lines)= @$_;
		defined $_ && chomp($_) for @lines;
		for (@lines) {
			$out .= sprintf("%s%*s | %s\n",
				!defined $re? '+' : !defined $_? '-' : ' ',
				-$widest_left, defined $re? $re : '',
				defined $_? output_text_to_qstr($_) : '');
		}
	}
	return $out;
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
sub output_text_to_qstr {
	my $string= shift;
	sub esc {
		defined $escapes{$_}? $escapes{$_} : sprintf("\\x%02X", ord)
	}
	return '""' unless length $string;
	return join "\n.", map { s/[\$\@\"\\\x00-\x1F\x7F-\xFF]/esc/eg; qq{"$_\\n"} }
		split /\n/, $string;
}

1;
