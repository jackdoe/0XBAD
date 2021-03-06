use Benchmark ':all';

use BAD;
use Cache::Memcached::Fast;
my $memd = new Cache::Memcached::Fast({servers => [ { address => 'localhost:11211', weight => 2.5 } ]});
my $bad = BAD->new(0xBEEF,16,128);
my %hash;

for my $key(qw(a aaaaaaaaaaaaaaaabbbbbbbbbbbbbbb)) {
    for my $i(qw(100)) {
        my $value = "a" x int($i);
        my $r = timethese($count, {
            "$i $key - memcached fast store" => sub { $memd->set($key,$value)},
            "$i $key - 0xbad store" => sub { $bad->store($key,$value,0)},
            "$i $key - native hash store" => sub { $hash{$key} = $value },
        });
        cmpthese($r); 

        $r = timethese($count, {
            "$i $key - memcached fast get" => sub { my $x = $memd->get($key)},
            "$i $key - 0xbad get" => sub { $x = $bad->find($key)},
            "$i $key - native hash get" => sub { my $x = $hash{$key}},
        });
        cmpthese($r); 
        my $missing = $key . "x";
        $r = timethese($count, {
            "$i $key - memcached fast miss" => sub { my $x = $memd->get($missing)},
            "$i $key - 0xbad miss" => sub { $x = $bad->find($missing)},
            "$i $key - native hash miss" => sub { my $x = $hash{$missing}},
        });
        cmpthese($r); 
        print "*"x80 . "\n";
    }
}
