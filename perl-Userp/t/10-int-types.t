use Test2::V0;
use Userp::Type::Integer;

my $int= Userp::Type::Integer->new( name => 'Int', id => 0 );
is( $int->min, undef, 'no min' );
is( $int->max, undef, 'no max' );
is( $int->bits, undef, 'no twos setting' );
#TODO: check bitsize attributes

my $pos= $int->subtype( name => 'Positive', id => 0, min => 0 );
is( $pos->min, 0, 'min = 0' );
is( $pos->max, undef, 'no max' );

my $neg= $int->subtype( name => 'Negative', id => 0, max => -1 );
is( $neg->min, undef, 'no min' );
is( $neg->max, -1, 'max = -1' );

my $int8u= $int->subtype( name => 'int8u', id => 0, min => 0, max => 255 );
is( $int8u->min, 0, 'min=0' );
is( $int8u->max, 255, 'max=FF' );

my $signed8pos= $int8u->subtype( name => '', id => 0, bits => 7 );
is( $signed8pos->min, 0, 'min=0' );
is( $signed8pos->max, 127, 'max=127' );

my $max1000= $signed8pos->subtype( name => '', id => 0, max => 1000 );
is( $max1000->min, 0, 'min=0' );
is( $max1000->max, 1000, 'max=1000' );
is( $max1000->bits, undef, 'twos setting was removed' );

done_testing;
