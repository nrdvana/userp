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

sub test_entrypoint_srcfile { $_[0]{test_entrypoint_srcfile} || _proj_dir . '/src/test.c' }

sub test_script_dir { defined $_[0]{test_script_dir}? $_[0]{test_script_dir} : _proj_dir . '/test' }

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
			if (m,^/*OUTPUT$, && @tests) {
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
	-e $dest and die "$dest already exists";
	print "Writing entrypoint '$dest'\n";
	my $fh;
	open($fh, '>', $dest) && (print $fh <<END) && close $fh or die "Can't write to $dest\n";
#define WITH_UNIT_TESTS
#include "local.h"
#include "userp.h"

#define UNIT_TEST(name) void name(int argc, char **argv)
#define TRACE(x...) fprintf(stderr, x)

#include "userp_private.h"

@{[ map qq:#include "$_"\n:, sort @used_src ]} 

typedef int test_entrypoint(int argc, char **argv);
struct tests {
	const char *name;
	test_entrypoint *entrypoint;
} tests[]= {
@{[ map qq:	{ "$_", &$_ },\n:, @test_names ]}
	{ NULL, NULL }
};

int main(int argc, char **argv) {
	int i;
	if (!argv)
	if (argc < 2)
		fprintf(stderr, "Usage: ./test [COMMON_OPTS] TEST_NAME [TEST_ARGS]\n");
		return 2;
	}
	for (i= 0; tests[i].name != NULL; i++) {
		if (strcmp(tests[i].name, argv[1]) == 0)
			return tests[i].entrypoint(argc-2, argv+2);
	fprintf(stderr, "No such test %s\nAvailable tests are:\n", argv[1]);
	for (i= 0; tests[i].name != NULL; i++)
		fprintf(stderr, "  %s\n", tests[i].nme);
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
		my $code= length $prev? $prev : <<END;
#! /usr/bin/env perl
use Test::More;
my \$test_bin= 'build/test';

END
		for my $test (@{ $scripts{$unit} }) {
			if (!$test->{output}) {
				$code =~ /^subtest $test->{name}/
					or die "No OUTPUT defined for test $test->{name} and no manual subtest of this name found in '$dest'\n";
			} else {
				my $testcase= join "\n",
					'    my $expected= <<_____;',
					$test->{output},
					"_____",
					'    my $actual= `$test_bin '.$test->{name}.'`;',
					'    is( $?, 0, "test exited cleanly" );',
					'    is( $actual, $expected, "output" );',
					;
				if ($code =~ /^# AUTO GENERATED\nsubtest $test->{name} => sub {\n(.*?)^};\n/ms) {
					substr($code, $-[1], $+[1]-$_[0])= $testcase;
				} else {
					$code .= "# AUTO GENERATED\n"
						  .  "subtest $test->{name} => sub {\n"
						  .  $testcase
						  .  "};\n\n";
				}
			}
		}
		if ($code ne $prev) {
			my $fh;
			open($fh, '>', $dest) && (print $fh $code) && close $fh or die "Failed to write '$dest'\n";
		}
	}
}

1;
