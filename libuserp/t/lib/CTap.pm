package CTap;
use strict;
use warnings;
use Carp;

sub new {
	my $class= shift;
	my %args= @_ == 1 && ref $_[0] eq 'HASH'? %{$_[0]} : @_;
	bless \%args, $class;
}

sub code { shift->{code} }
sub subtests { shift->{subtests} ||= [] }

sub load {
	my $self= shift;
	for (@_ == 1 && ref $_[0] eq 'ARRAY'? @{$_[0]} : @_) {
		open my $fh, '<:utf8', $_ or croak "open($_): $!";
		while (<$fh>) {
			if ($_ =~ /^SUBTEST\((.*)\)/) {
				my ($fn, $plan)= split /,/, $1;
				$_ =~ s/^SUBTEST\((.*)\)/SUBTEST\($fn\)/; # remove plan from macro, to simplify things
				push @{ $self->subtests }, { name => $fn, plan => $plan };
			}
			$self->{code} .= $_;
		}
	}
	return $self;
}

sub write {
	my ($self, $dest)= @_;
	rename($dest, $dest.'.old') or croak "rename -> $dest.old: $!"
		if -e $dest;
	my $subtest_decl= join "\n",
		map 'void '.$_->{name}.'(TAP_state_t *state);',
			@{ $self->subtests };
	my $subtest_init= join ",\n",
		map '  { .fn= &test_'.$_->{name}.', .name="'.$_->{name}.'", .plan_count='.($_->{plan}||0).' }',
			@{ $self->subtests };
	my $code= <<"END";

#include "CTap.h"

${\$self->code}

int TAP_subtest_count= ${\scalar @{$self->subtests} };

TAP_subtest_t TAP_subtests[]= {
$subtest_init
};

#include "CTap.c"

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
