package CTap;
use strict;
use warnings;
use Carp;
use File::Spec;

=head1 SYNOPSIS

  use CTap;
  CTap->new->include(@source_file_paths)->write($generated_c_path);

=head1 DESCRIPTION

This module reads a list of source files, and generates a new source file that can be passed to
gcc or shipped with a distribution.  Each source file is scanned for C<< SUBTEST(...){...} >>
blocks.  These blocks are converted to C functions and indexed in static data so that the
generated binary is able to list its own available test cases.

If an input source file does not have any SUBTEST directives, it can be referenced with an
C<< #include >> directive rather than inserted in-line into the generated code.

=head1 ATTRIBUTES

=head2 inline

If set to true, the generated source will always contain the entirety of input files it was
given, rather than referring to them with C<< #include >> directives.

=head2 includes

An array of files being included into the generated code.

=head2 subtests

An array holding the list of subtests discovered so far in the included source files.

=cut

sub new {
	my $class= shift;
	my %args= @_ == 1 && ref $_[0] eq 'HASH'? %{$_[0]} : @_;
	bless \%args, $class;
}

sub ctap_h_path { $_[0]{ctap_h_path} ||= do { my $x= __FILE__; $x =~ s|[^/\\]+$|CTap.h|; $x } }
sub ctap_c_path { $_[0]{ctap_c_path} ||= do { my $x= __FILE__; $x =~ s|[^/\\]+$|CTap.c|; $x } }
sub inline      { $_[0]{inline} }
sub includes    { $_[0]{includes} ||= [] }
sub subtests    { $_[0]{subtests} ||= [] }

sub include {
	my $self= shift;
	for my $fname (@_ == 1 && ref $_[0] eq 'ARRAY'? @{$_[0]} : @_) {
		open my $fh, '<:utf8', $fname or croak "open($_): $!";
		my $src= '';
		while (<$fh>) {
			if ($_ =~ /^SUBTEST\((.*)\)/) {
				my ($fn, $plan)= split /,/, $1;
				push @{ $self->subtests }, { name => $fn, plan => $plan };
			}
			$src .= $_;
		}
		push @{$self->includes}, { path => $fname, code => $_ };
	}
	return $self;
}

sub write {
	my ($self, $dest)= @_;
	# Sanity check on $dest
	croak "Must specify a file name (not directory) in CTap->write: '$dest'"
		if -d $dest;
	rename($dest, $dest.'.old') or croak "rename -> $dest.old: $!"
		if -e $dest;
	
	# Calculate dest dir, which paths will be relative to
	(my $dest_dir= $dest) =~ s|[^/\\]+$||;
	$dest_dir= File::Spec->rel2abs($dest_dir);
	
	# Build variables needed for the template
	my $subtest_count= @{ $self->subtests };
	my $subtest_decl= join "\n",
		map 'void '.$_->{name}.'(TAP_state_t *state);',
			@{ $self->subtests };
	my $subtest_init= join ",\n",
		map '  { .fn= &test_'.$_->{name}.', .name="'.$_->{name}.'", .plan_count='.($_->{plan}||0).' }',
			@{ $self->subtests };
	my $includes= join "\n", map {
		my $rel_path= File::Spec->abs2rel(File::Spec->rel2abs($_->{path}), $dest_dir);
		$self->inline? qq|#line 0 "$rel_path"\n$_->{code}|
			: qq|#include "$rel_path"|;
		} @{ $self->includes };
	my $ctap_h_rel_path= File::Spec->abs2rel(File::Spec->rel2abs($self->ctap_h_path), $dest_dir);
	my $ctap_c_rel_path= File::Spec->abs2rel(File::Spec->rel2abs($self->ctap_c_path), $dest_dir);

	my $code= <<"END";
#include "$ctap_h_rel_path"

$includes

int TAP_subtest_count= $subtest_count;

TAP_subtest_t TAP_subtests[]= {
$subtest_init
};

#include "$ctap_c_rel_path"

int main(int argc, char **argv) {
	return TAP_main(argc, argv);
}

END
	open my $fh, '>:utf8', $dest or croak "open($dest): $!";
	print $fh $code
		and close $fh
		or do { my $err= $!; unlink $dest; croak "write($dest): $err"; };
	$self;
}

1;
