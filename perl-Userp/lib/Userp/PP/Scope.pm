package Userp::PP::Scope;
use Moo;
use Carp;

has id             => ( is => 'ro', required => 1 );
has parent         => ( is => 'ro', required => 1 );
has block_type     => ( is => 'ro', required => 1 );
has anon_blocks    => ( is => 'ro', required => 1 );
has final          => ( is => 'ro', required => 1 );
has idents         => ( is => 'ro', required => 1 );
has first_ident_id => ( is => 'rwp' ); # initialized by BUILD
sub total_ident_count { $_[0]->first_ident_id + @{ $_[0]->idents } - 1 }
has types          => ( is => 'ro', required => 1 );
has first_type_id  => ( is => 'rwp' ); # initialized by BUILD
sub total_type_count  { $_[0]->first_type_id + @{ $_[0]->types } - 1 }
has _ident_by_id   => ( is => 'ro', default => sub { {} } );
has _ident_by_val  => ( is => 'ro', default => sub { {} } );
has _type_table    => ( is => 'ro', default => sub { [] } );

sub BUILD {
	my $self= shift;
	my $id= $self->parent? $self->parent->total_ident_count+1 : 1; # list is 1-based
	$self->_set_first_ident_id($id);
	# The Ident table could be quite large, and it needs deep-cloned, so only initialize these
	# and then import the rest from parent lazily.
	for (@{ $self->idents }) {
		$self->_ident_by_val->{$_}=
			$self->_ident_by_id->{$id}=
				[ $id, $_, undef ];
		++$id;
	}
	# The type table is probably smaller, and doesn't need deep-cloned, so just copy it from parent.
	my $types= $self->_type_by_id;
	@$types= $self->parent? @{ $self->parent->_type_table } : (undef); # list is 1-based.
	$self->_set_first_type_id(scalar @$types);
	push @$types, @{ $self->types };
	for my $type (@{ $self->types }) {
		my $ident= $type->ident;
		# A record for this identifier should exist, unless it already existed in parent.
		my $rec= $self->_ident_by_val->{$ident};
		if ($rec) {
			$rec->[3]= $type;
		}
		else {
			my $id= $self->parent->ident_id($ident)
				or croak "Identifier for type $ident does not exist in current scope";
			$self->_ident_by_val->{$ident}=
				$self->_ident_by_id->{$id}= [ $id, $ident, $type ];
		}
	}
}

sub _ident_info {
	my $rec= defined $_[1]? $_[0]{_ident_by_id}{$_[1]} : $_[0]{_ident_by_val}{$_[2]};
	if (!$rec && $_[0]->parent && ($rec= $_[0]->parent->_ident_info($_[1], $_[2]))) {
		$rec= [ @$rec ];
		$_[0]{_ident_by_id}{$rec->[0]}= $rec;
		$_[0]{_ident_by_val}{$rec->[1]}= $rec;
	}
	return $rec;
}

sub ident_id {
	my $rec= $_[0]->_ident_info(undef, $_[1]);
	return $rec? $rec->[0] : undef;
}

sub ident_by_id {
	my $rec= $_[0]->_ident_info($_[1]);
	return $rec->[0] if defined $rec;
	croak "IdentID out of range: $_[1]";
}

sub type_by_id {
	my $t= $_[1] > 0? $_[0]{_type_table}{$_[1]} : undef;
	return $t if defined $t;
	croak "TypeID out of range: $_[1]";
}

sub type_by_ident {
	my $rec= $_[0]->_ident_info($_[1]);
	return $rec? $rec->[2] : undef;
}

1;
