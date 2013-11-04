use strict;
use warnings;
use BAD; 
use Test::More tests => 3;
use Data::Dumper;
my $val = 'a' x 100;

my $b = BAD->new(0xBAD,16);

is $val, $b->store("bzbz",$val);

is $b->find("bzbz"), $val;
is $b->find("bzbzz"), undef;
