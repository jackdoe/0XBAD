# encoding: utf-8
require 'test/unit'
require 'sereal'
require ENV['USE_CURRENT_DIRECTORY'] ? File.absolute_path(File.join(File.dirname(__FILE__),'..','lib','xbad')) : 'xbad'

class Test::Unit::TestCase
    def test_everything
        x = XBAD.create(0xBAD,16,128);
        x.store("key","value",0)
        assert_equal x.find("key"), "value"

        complex = { 1 => [1,2,3,4,"aaa"], "b" => "complex"}
        x.store("key2",Sereal.encode(complex),0)
        assert_equal Sereal.decode(x.find("key2")),complex
    end
end
