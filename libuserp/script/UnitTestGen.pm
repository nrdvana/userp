package UnitTestGen;
use strict;
use Getopt::Long;
use Pod::Usage;

=head1 SYNOPSIS

  perl -I. -MUnitTestGen -e UnitTestGen::run [OPTIONS] SOURCE_FILE ...

=head1 OPTIONS

=over

=item --entrypoint=SRCFILE

Write a test entrypoint to SRCFILE

=item --testdir=PATH

Write test scripts to this PATH

=item SOURCE_FILE

Scan these source files for unit tests

=back

=cut

sub parse_options {
	my $self= shift;
	my $opt= Getopt::Long::Parser->new;
	$opt->configure(qw( permute ));
	return $opt->getoptionsfromarray(\@_,
		'entrypoint=s' => \$self->{test_entrypoint_srcfile},
		'testdir=s'    => \$self->{test_script_dir},
		'<>'           => sub { push @{ $self->{source_files} }, shift },
		'help|?'       => sub { Pod::Usage::pod2usage(1) },
	);
}


=head1 DESCRIPTION

This module reads C source files looking for UNIT_TEST markers, then generates a test.c
entrypoint that can invoke all of those tests, then generates perl TAP test scripts that
verify the output of each test.

=head1 ATTRIBUTES

=head2 source_files

An ordered list of C source files which will be searched for UNIT_TESTs.

=head2 test_entrypoint_srcfile

The (path to) test.c source file that will be written.

=head2 test_script_dir

The path where perl test scripts will be written.

=cut

sub _proj_dir {
	my $path= __FILE__;
	$path =~ s,[\\/]?UnitTestGen\.pm$,,;
	$path= '.' unless length $path;
	$path =~ s,[\\/]?script$,, or $path .= '/..';
	$path= '.' unless length $path;
	$path;
}

sub source_files { $_[0]{source_files} || [] }

sub test_entrypoint_srcfile { $_[0]{test_entrypoint_srcfile} || _proj_dir . '/src/unittest.c' }

sub test_script_dir { defined $_[0]{test_script_dir}? $_[0]{test_script_dir} : _proj_dir . '/t' }

=head2 tests

A list of tests found in the source_files

=cut

sub tests {
	$_[0]{tests} ||= $_[0]->scan_source_files;
}

=head1 METHODS

=head2 run

  UnitTestGen::run; # uses ARGV as options
  UnitTestGen::run(@options);
  UnitTestGen->new(%attributes)->run;

Run the generator, scanning the source files, generating the entrypoint, and
updating the test scripts.  

=cut

sub new {
	my $class= shift;
	bless { ref $_[0] eq 'HASH'? %{$_[0]} : @_ }, $class
}

sub run {
	my $self;
	if (ref($_[0]) && ref($_[0])->isa(__PACKAGE__)) {
		$self= shift;
	} else {
		$self= __PACKAGE__->new;
		unshift @_, @ARGV unless @_;
	}
	if (@_) {
		$self->parse_options(@_) or Pod::Usage::pod2usage(2);
	}
	$self->scan_source_files;
	$self->write_entrypoint;
	$self->update_testscripts;
}

sub scan_source_files {
	my $self= shift;
	my @tests;
	for my $fname (@{ $self->source_files }) {
		open my $fh, '<', $fname or die "Can't open $_\n";
		while (<$fh>) {
			push @tests, { name => $1, file => $fname } if /^UNIT_TEST\((\w+)\)/;
			if (m,^/[*]OUTPUT$, && @tests) {
				$tests[-1]{output}= '';
				while (<$fh>) {
					last if m,[*]/,;
					$tests[-1]{output} .= $_;
				}
			}
		}
	}
	$self->{tests}= \@tests;
}

sub write_entrypoint {
	my $self= shift;
	my %seen;
	my @used_src= grep !$seen{$_}++, map $_->{file}, @{ $self->tests };
	my @test_names= sort map $_->{name}, @{ $self->tests };
	my $dest= $self->test_entrypoint_srcfile;
	if (-e $dest) {
		my $prev= do { local($/,@ARGV)= (undef,$dest); <> };
		$prev =~ /test_entrypoint/
			or die "$dest already exists, and does not look like a generated test entrypoint";
	}
	print "Writing entrypoint '$dest'\n";
	my $fh;
	open($fh, '>', $dest) && (print $fh <<END) && close $fh or die "Can't write to $dest\n";
#include "local.h"
#define UNIT_TEST(name) void name(int argc, char **argv)
#define TRACE(env, x...) do{ if(env->log_trace){ userp_diag_set(env->diag,x); userp_env_emit_diag(env); } }while(0)
#include "userp_private.h"

@{[ map qq:#include "$_"\n:, sort @used_src ]} 

typedef void test_entrypoint(int argc, char **argv);
struct tests {
	const char *name;
	test_entrypoint *entrypoint;
} tests[]= {
@{[ map qq:	{ "$_", &$_ },\n:, @test_names ]}
	{ NULL, NULL }
};

int main(int argc, char **argv) {
	int i;
	if (argc < 2) {
		fprintf(stderr, "Usage: ./test [COMMON_OPTS] TEST_NAME [TEST_ARGS]\\n");
		return 2;
	}
	for (i= 0; tests[i].name != NULL; i++)
		if (strcmp(tests[i].name, argv[1]) == 0) {
			tests[i].entrypoint(argc-2, argv+2);
			return 0;
		}
	fprintf(stderr, "No such test %s\\nAvailable tests are:\\n", argv[1]);
	for (i= 0; tests[i].name != NULL; i++)
		fprintf(stderr, "  %s\\n", tests[i].name);
	return 2;
}
END
}

sub update_testscripts {
	my $self= shift;
	my $dir= $self->test_script_dir;
	my %scripts;
	for (@{ $self->tests }) {
		my $unit= $_->{file};
		$unit =~ s/.*?(\w+)[.]c$/$1/;
		push @{ $scripts{$unit} ||= [] }, $_;
	}
	for my $unit (sort keys %scripts) {
		my $dest= "$dir/$unit.t";
		print "Updating $dest\n";
		my $prev= -e $dest? do { local ($/,@ARGV)=(undef,$dest); <> } : '';
		my $code= length $prev? $prev : <<'END';
#! /usr/bin/env perl
use Test::More;
use FindBin;
use lib "$FindBin::Bin/lib";
use TestUserp;

END
		$code =~ s/^done_testing$//m;
		for my $test (@{ $scripts{$unit} }) {
			if (!$test->{output}) {
				$code =~ /^subtest $test->{name}/
					or die "No OUTPUT defined for test $test->{name} and no manual subtest of this name found in '$dest'\n";
			} else {
				my $expected= $test->{output};
				my $delim_word= '---';
				$expected =~ s/^/      /mg;
				my $testcase= join "\n",
					"    check_unittest_output($test->{name}, unindent <<'$delim_word');",
					$expected
					.$delim_word;
				if ($code =~ /^# AUTO GENERATED\nsubtest $test->{name} => sub \{\n(.*?)\n\};\n\n/ms) {
					substr($code, $-[1], $+[1]-$-[1])= $testcase;
				} else {
					$code .= join "\n",
						"# AUTO GENERATED",
						"subtest $test->{name} => sub {",
						$testcase,
						"};\n\n";
				}
			}
		}
		$code .= "done_testing\n";
		if ($code ne $prev) {
			my $fh;
			open($fh, '>', $dest) && (print $fh $code) && close $fh or die "Failed to write '$dest'\n";
		}
	}
}

sub output_text_to_perl {
	my $string= shift;
	my %escapes= (
		"\n" => '\\n',
		"\t" => '\\t',
		'$' => '\\$',
		'@' => '\\@',
	);
	sub esc {
		defined $escapes{$_}? $escapes{$_} : sprintf("\\x%02X", ord)
	}
	return ' '.join "\n.", map { s/[\$\@\"\\\x0-\x1F\x7F-\xFF]/esc/eg; qq{"$_\\n"} }
		split /\n/, $string;
}

1;
