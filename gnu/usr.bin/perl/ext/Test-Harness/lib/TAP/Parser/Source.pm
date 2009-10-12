package TAP::Parser::Source;

use strict;
use vars qw($VERSION @ISA);

use TAP::Object                  ();
use TAP::Parser::IteratorFactory ();

@ISA = qw(TAP::Object);

# Causes problem on MacOS and shouldn't be necessary anyway
#$SIG{CHLD} = sub { wait };

=head1 NAME

TAP::Parser::Source - Stream output from some source

=head1 VERSION

Version 3.17

=cut

$VERSION = '3.17';

=head1 SYNOPSIS

  use TAP::Parser::Source;
  my $source = TAP::Parser::Source->new;
  my $stream = $source->source(['/usr/bin/ruby', 'mytest.rb'])->get_stream;

=head1 DESCRIPTION

Takes a command and hopefully returns a stream from it.

=head1 METHODS

=head2 Class Methods

=head3 C<new>

 my $source = TAP::Parser::Source->new;

Returns a new C<TAP::Parser::Source> object.

=cut

# new() implementation supplied by TAP::Object

sub _initialize {
    my ( $self, $args ) = @_;
    $self->{switches} = [];
    _autoflush( \*STDOUT );
    _autoflush( \*STDERR );
    return $self;
}

##############################################################################

=head2 Instance Methods

=head3 C<source>

 my $source = $source->source;
 $source->source(['./some_prog some_test_file']);

 # or
 $source->source(['/usr/bin/ruby', 't/ruby_test.rb']);

Getter/setter for the source.  The source should generally consist of an array
reference of strings which, when executed via L<&IPC::Open3::open3|IPC::Open3>,
should return a filehandle which returns successive rows of TAP.  C<croaks> if
it doesn't get an arrayref.

=cut

sub source {
    my $self = shift;
    return $self->{source} unless @_;
    unless ( 'ARRAY' eq ref $_[0] ) {
        $self->_croak('Argument to &source must be an array reference');
    }
    $self->{source} = shift;
    return $self;
}

##############################################################################

=head3 C<get_stream>

 my $stream = $source->get_stream;

Returns a L<TAP::Parser::Iterator> stream of the output generated by executing
C<source>.  C<croak>s if there was no command found.

Must be passed an object that implements a C<make_iterator> method.
Typically this is a TAP::Parser instance.

=cut

sub get_stream {
    my ( $self, $factory ) = @_;
    my @command = $self->_get_command
      or $self->_croak('No command found!');

    return $factory->make_iterator(
        {   command => \@command,
            merge   => $self->merge
        }
    );
}

sub _get_command { return @{ shift->source || [] } }

##############################################################################

=head3 C<merge>

  my $merge = $source->merge;

Sets or returns the flag that dictates whether STDOUT and STDERR are merged.

=cut

sub merge {
    my $self = shift;
    return $self->{merge} unless @_;
    $self->{merge} = shift;
    return $self;
}

# Turns on autoflush for the handle passed
sub _autoflush {
    my $flushed = shift;
    my $old_fh  = select $flushed;
    $| = 1;
    select $old_fh;
}

1;

=head1 SUBCLASSING

Please see L<TAP::Parser/SUBCLASSING> for a subclassing overview.

=head2 Example

  package MyRubySource;

  use strict;
  use vars '@ISA';

  use Carp qw( croak );
  use TAP::Parser::Source;

  @ISA = qw( TAP::Parser::Source );

  # expect $source->(['mytest.rb', 'cmdline', 'args']);
  sub source {
    my ($self, $args) = @_;
    my ($rb_file) = @$args;
    croak("error: Ruby file '$rb_file' not found!") unless (-f $rb_file);
    return $self->SUPER::source(['/usr/bin/ruby', @$args]);
  }

=head1 SEE ALSO

L<TAP::Object>,
L<TAP::Parser>,
L<TAP::Parser::Source::Perl>,

=cut

