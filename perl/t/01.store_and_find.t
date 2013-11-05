use strict;
use warnings;
use BAD; 
use Test::More tests => 5;
use Data::Dumper;
my $val = 'a' x 100;

my $b = BAD->new(0xBAD,16,128);

is $val, $b->store("bzbz",$val,1);

is $b->find("bzbz"), $val;
is $b->find("bzbzz"), undef;
sleep 1;
is $b->find("bzbz"), undef;
eval {
    $b->store("b",'a' x 128,0);
};
like $@,qr{unable to store};

