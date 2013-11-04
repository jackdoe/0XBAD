package BAD;

use 5.014002;
use strict;
use warnings;

require Exporter;

our @ISA = qw(Exporter);
our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	
);

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('BAD', $VERSION);
1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

BAD - Perl extension for simplified cache

=head1 SYNOPSIS

  use BAD;
=head1 DESCRIPTION

see the README file

=head2 EXPORT

None by default.



=head1 SEE ALSO

=head1 AUTHOR

borislav nikolov, E<lt>jack@sofialondonmoskva.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2013 by borislav nikolov

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.14.2 or,
at your option, any later version of Perl 5 you may have available.


=cut
