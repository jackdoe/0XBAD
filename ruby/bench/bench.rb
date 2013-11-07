begin
      require 'xbad'
rescue LoadError
      require './lib/xbad'
end
require 'sereal'
require 'benchmark/ips'
n = 1_000_000
b = XBAD.create(0xBAD,16,128)
key = "abcdef"
missing = "abcdefg"
value = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
hash = {}

complex = { 1 => [1,2,3,4,"aaa"], "b" => "complex"}

Benchmark.ips do |x|
    x.report("hash set") { hash[key] = value  }
    x.report("xbad set") { b.store(key,value,0) }
    x.report("hash get") { z = hash[key] }
    x.report("xbad get") { z = b.find(key) }
    x.report("hash mis") { z = hash[missing] }
    x.report("xbad mis") { z = b.find(missing) }
    x.report("xbad set-encoded") { b.store(key,Sereal.encode(complex),0) }
    x.report("xbad get-encoded") { z = Sereal.decode(b.find(key)) }
    x.report("xbad set-encoded-sn") { b.store(key,Sereal.encode(complex,true),0) }
    x.report("xbad get-encoded-sn") { z = Sereal.decode(b.find(key)) }
end

