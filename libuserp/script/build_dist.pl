#! /usr/bin/env perl
use strict;
use warnings;
use FindBin;
use File::stat;
use lib $FindBin::Bin;
use File::Spec;
use Getopt::Long;
use Pod::Usage;
use Cwd;
sub src_path; sub dst_path; sub make; sub freshen_file; sub mk_path;

=head1 SYNOPSIS

  # assembles a build-ready tree of files in TARGET_DIR
  script/build_dist.pl -d TARGET_DIR [OPTIONS]

=head1 OPTIONS

=over

=item -d TARGET_DIR

=item --dest TARGET_DIR

Create a distribtion at this path.  If the directory exists, it will be freshened

=item --clobber

Wipe the target directory first

=item --symlink

Create symlinks from src/ into the dist directory, instead of copying files.  This allows you
to edit the src/ files and have the changes seen in the build directory.

=item --pregenerate

Generate all files which are dynamically generated, instead of waiting for the makefile to
build them on demand.

=item --tarball

After generating all files, create a distribution tarball.

=back

=cut

my $opt_ok= GetOptions(
	'd|dest=s'    => \my $opt_dest,
	'clobber'     => \my $opt_clobber,
	'symlink'     => \my $opt_symlink,
	'tarball'     => \my $opt_tarball,
	'pregenerate' => \my $opt_pregenerate,
	'help|?'      => sub { pod2usage(1) },
) or pod2usage(2);

my $proj_root= Cwd::abs_path("$FindBin::Bin/..");

if ($opt_clobber && -e $opt_dest) {
	-d $opt_dest or die "'$opt_dest' is not a directory; refusing to clean.\n";
	system('rm','-r',$opt_dest) == 0 or die "rm failed\n";
}

# Create destination

mk_path $opt_dest;
$opt_dest= Cwd::abs_path($opt_dest);

# Freshen source files into root of destination

for (<$proj_root/src/*>) {
	my ($fname)= ($_ =~ m|([^/\\]+)$|);
	freshen_file($_, dst_path($fname));
}
mk_path(dst_path('t'));
for (<$proj_root/t/*>) {
	my ($fname)= ($_ =~ m|([^/\\]+)$|);
	freshen_file($_, dst_path('t',$fname));
}	

# Install autoconf files

for (qw( Makefile.am configure.ac dev_rules.mak )) {
	freshen_file(src_path('script','automake',$_), dst_path($_));
}
system('autoreconf','-s','-i',dst_path) == 0 or die "autoreconf failed\n";

# Build the makefile if it doesn't exist.  If it does exist it should rebuild itself.

unless ( -f dst_path('Makefile') ) {
	my $config= -f dst_path('config.status')? './config.status' : './configure';
	system(qq|cd "$opt_dest" && $config|) == 0 or die "$config failed\n";
}

# Pre-generate all the built sources, and then remove the makefile rules that generate them.
if ($opt_tarball || $opt_pregenerate) {
	make 'autogen_files';
	unlink dst_path('dev_rules.mak');
	open my $fh, '>', dst_path('dev_rules.mak');
}

if ($opt_tarball) {
	# Verify we have all changes checked in

	my @uncommitted= `git --git-dir="$proj_root/.git" status --porcelain`;
	!@uncommitted
		or die "Uncommitted git changes!";
	
	# Run test cases to ensure things are ready
	
	make 'autogen_files';
	make 'test';
	
	# clean one last time, then zip the directory
	
	make 'distclean';
	...;
}

#------------------- utility functions ----------------

sub freshen_file {
	my ($src, $dst)= @_;
	my $src_info= stat $src or die "Can't read '$src'\n";
	my $dst_info= stat $dst;
	return 1 if $dst_info && $src_info->size == $dst_info->size && $src_info->mtime == $dst_info->mtime;
	unlink $dst if $dst_info;
	if ($opt_symlink) {
		symlink(File::Spec->abs2rel($src, $opt_dest), $dst) or die "symlink($dst): $!\n";
	} else {
		system('cp','-a', $src, $dst) == 0 or die "cp to '$dst' failed\n";
	}
}

sub make {
	system('make', '-C', $opt_dest, @_) == 0
		or die "make failed";
}

sub src_path {
	File::Spec->join($proj_root, @_)
}

sub dst_path {
	File::Spec->join($opt_dest, @_)
}

sub mk_path {
	my $path= shift;
	system('mkdir','-p',$path) == 0 or die "mkdir -p '$path' failed\n";
}
