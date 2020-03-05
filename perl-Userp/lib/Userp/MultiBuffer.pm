package Userp::MultiBuffer;
use strict;
use warnings;
use Userp::Buffer;

sub new {
	my $class= shift;
	my @array= @_ == 1 && ref $_[0] eq 'ARRAY'? @{$_[0]} : @_;
	bless \@array, $class;
}

sub new_le {
	my $class= shift;
	bless [ Userp::Buffer->new_le ], $class;
}

sub new_be {
	my $class= shift;
	bless [ Userp::Buffer->new_be ], $class;
}

sub to_string {
	my ($self, $align, $offset)= @_;
	return '' unless @$self;
	$offset ||= 0;
	my $result= $self->[0]->new_same("\0"x$offset, $align);
	$result->append_buffer($_) for @$self;
	return substr(${$result->bufref}, $offset);
}

1;
