Gem::Specification.new do |s|
    s.name         = "xbad"
    s.version      = "0.0.4"
    s.summary      = "xbad - shared memory key/value cache"
    s.description  = "xbad - shared memory key/value cache using shared memory and udp broadcasts for distribution"
    s.homepage     = "https://github.com/jackdoe/0XBAD"
    s.authors      = ["Borislav Nikolov"]
    s.email        = "jack@sofialondonmoskva.com"
    s.files        = Dir.glob("ext/**/*.{c,h,rb}") + Dir.glob("lib/**/*.rb")
    s.license      = 'MIT'
    s.extensions << "ext/xbad/extconf.rb"
    s.add_development_dependency "rake-compiler"
end
