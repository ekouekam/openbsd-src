
# !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
# This file was created by warnings.pl
# Any changes made here will be lost.
#

package warnings;

our $VERSION = '1.03';

=head1 NAME

warnings - Perl pragma to control optional warnings

=head1 SYNOPSIS

    use warnings;
    no warnings;

    use warnings "all";
    no warnings "all";

    use warnings::register;
    if (warnings::enabled()) {
        warnings::warn("some warning");
    }

    if (warnings::enabled("void")) {
        warnings::warn("void", "some warning");
    }

    if (warnings::enabled($object)) {
        warnings::warn($object, "some warning");
    }

    warnings::warnif("some warning");
    warnings::warnif("void", "some warning");
    warnings::warnif($object, "some warning");

=head1 DESCRIPTION

The C<warnings> pragma is a replacement for the command line flag C<-w>,
but the pragma is limited to the enclosing block, while the flag is global.
See L<perllexwarn> for more information.

If no import list is supplied, all possible warnings are either enabled
or disabled.

A number of functions are provided to assist module authors.

=over 4

=item use warnings::register

Creates a new warnings category with the same name as the package where
the call to the pragma is used.

=item warnings::enabled()

Use the warnings category with the same name as the current package.

Return TRUE if that warnings category is enabled in the calling module.
Otherwise returns FALSE.

=item warnings::enabled($category)

Return TRUE if the warnings category, C<$category>, is enabled in the
calling module.
Otherwise returns FALSE.

=item warnings::enabled($object)

Use the name of the class for the object reference, C<$object>, as the
warnings category.

Return TRUE if that warnings category is enabled in the first scope
where the object is used.
Otherwise returns FALSE.

=item warnings::warn($message)

Print C<$message> to STDERR.

Use the warnings category with the same name as the current package.

If that warnings category has been set to "FATAL" in the calling module
then die. Otherwise return.

=item warnings::warn($category, $message)

Print C<$message> to STDERR.

If the warnings category, C<$category>, has been set to "FATAL" in the
calling module then die. Otherwise return.

=item warnings::warn($object, $message)

Print C<$message> to STDERR.

Use the name of the class for the object reference, C<$object>, as the
warnings category.

If that warnings category has been set to "FATAL" in the scope where C<$object>
is first used then die. Otherwise return.


=item warnings::warnif($message)

Equivalent to:

    if (warnings::enabled())
      { warnings::warn($message) }

=item warnings::warnif($category, $message)

Equivalent to:

    if (warnings::enabled($category))
      { warnings::warn($category, $message) }

=item warnings::warnif($object, $message)

Equivalent to:

    if (warnings::enabled($object))
      { warnings::warn($object, $message) }

=back

See L<perlmodlib/Pragmatic Modules> and L<perllexwarn>.

=cut

use Carp ();

our %Offsets = (

    # Warnings Categories added in Perl 5.008

    'all'		=> 0,
    'closure'		=> 2,
    'deprecated'	=> 4,
    'exiting'		=> 6,
    'glob'		=> 8,
    'io'		=> 10,
    'closed'		=> 12,
    'exec'		=> 14,
    'layer'		=> 16,
    'newline'		=> 18,
    'pipe'		=> 20,
    'unopened'		=> 22,
    'misc'		=> 24,
    'numeric'		=> 26,
    'once'		=> 28,
    'overflow'		=> 30,
    'pack'		=> 32,
    'portable'		=> 34,
    'recursion'		=> 36,
    'redefine'		=> 38,
    'regexp'		=> 40,
    'severe'		=> 42,
    'debugging'		=> 44,
    'inplace'		=> 46,
    'internal'		=> 48,
    'malloc'		=> 50,
    'signal'		=> 52,
    'substr'		=> 54,
    'syntax'		=> 56,
    'ambiguous'		=> 58,
    'bareword'		=> 60,
    'digit'		=> 62,
    'parenthesis'	=> 64,
    'precedence'	=> 66,
    'printf'		=> 68,
    'prototype'		=> 70,
    'qw'		=> 72,
    'reserved'		=> 74,
    'semicolon'		=> 76,
    'taint'		=> 78,
    'threads'		=> 80,
    'uninitialized'	=> 82,
    'unpack'		=> 84,
    'untie'		=> 86,
    'utf8'		=> 88,
    'void'		=> 90,
    'y2k'		=> 92,
  );

our %Bits = (
    'all'		=> "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x15", # [0..46]
    'ambiguous'		=> "\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00\x00", # [29]
    'bareword'		=> "\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00\x00\x00", # [30]
    'closed'		=> "\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [6]
    'closure'		=> "\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [1]
    'debugging'		=> "\x00\x00\x00\x00\x00\x10\x00\x00\x00\x00\x00\x00", # [22]
    'deprecated'	=> "\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [2]
    'digit'		=> "\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00\x00", # [31]
    'exec'		=> "\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [7]
    'exiting'		=> "\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [3]
    'glob'		=> "\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [4]
    'inplace'		=> "\x00\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00", # [23]
    'internal'		=> "\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00", # [24]
    'io'		=> "\x00\x54\x55\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [5..11]
    'layer'		=> "\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [8]
    'malloc'		=> "\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00\x00\x00", # [25]
    'misc'		=> "\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00", # [12]
    'newline'		=> "\x00\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [9]
    'numeric'		=> "\x00\x00\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00", # [13]
    'once'		=> "\x00\x00\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00", # [14]
    'overflow'		=> "\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00", # [15]
    'pack'		=> "\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00", # [16]
    'parenthesis'	=> "\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00", # [32]
    'pipe'		=> "\x00\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [10]
    'portable'		=> "\x00\x00\x00\x00\x04\x00\x00\x00\x00\x00\x00\x00", # [17]
    'precedence'	=> "\x00\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00", # [33]
    'printf'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00\x00", # [34]
    'prototype'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00", # [35]
    'qw'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00", # [36]
    'recursion'		=> "\x00\x00\x00\x00\x10\x00\x00\x00\x00\x00\x00\x00", # [18]
    'redefine'		=> "\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00", # [19]
    'regexp'		=> "\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00", # [20]
    'reserved'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00", # [37]
    'semicolon'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00", # [38]
    'severe'		=> "\x00\x00\x00\x00\x00\x54\x05\x00\x00\x00\x00\x00", # [21..25]
    'signal'		=> "\x00\x00\x00\x00\x00\x00\x10\x00\x00\x00\x00\x00", # [26]
    'substr'		=> "\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00", # [27]
    'syntax'		=> "\x00\x00\x00\x00\x00\x00\x00\x55\x55\x15\x00\x00", # [28..38]
    'taint'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00", # [39]
    'threads'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00", # [40]
    'uninitialized'	=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04\x00", # [41]
    'unopened'		=> "\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [11]
    'unpack'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00", # [42]
    'untie'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00", # [43]
    'utf8'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", # [44]
    'void'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04", # [45]
    'y2k'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10", # [46]
  );

our %DeadBits = (
    'all'		=> "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\x2a", # [0..46]
    'ambiguous'		=> "\x00\x00\x00\x00\x00\x00\x00\x08\x00\x00\x00\x00", # [29]
    'bareword'		=> "\x00\x00\x00\x00\x00\x00\x00\x20\x00\x00\x00\x00", # [30]
    'closed'		=> "\x00\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [6]
    'closure'		=> "\x08\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [1]
    'debugging'		=> "\x00\x00\x00\x00\x00\x20\x00\x00\x00\x00\x00\x00", # [22]
    'deprecated'	=> "\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [2]
    'digit'		=> "\x00\x00\x00\x00\x00\x00\x00\x80\x00\x00\x00\x00", # [31]
    'exec'		=> "\x00\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [7]
    'exiting'		=> "\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [3]
    'glob'		=> "\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [4]
    'inplace'		=> "\x00\x00\x00\x00\x00\x80\x00\x00\x00\x00\x00\x00", # [23]
    'internal'		=> "\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00", # [24]
    'io'		=> "\x00\xa8\xaa\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [5..11]
    'layer'		=> "\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [8]
    'malloc'		=> "\x00\x00\x00\x00\x00\x00\x08\x00\x00\x00\x00\x00", # [25]
    'misc'		=> "\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00", # [12]
    'newline'		=> "\x00\x00\x08\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [9]
    'numeric'		=> "\x00\x00\x00\x08\x00\x00\x00\x00\x00\x00\x00\x00", # [13]
    'once'		=> "\x00\x00\x00\x20\x00\x00\x00\x00\x00\x00\x00\x00", # [14]
    'overflow'		=> "\x00\x00\x00\x80\x00\x00\x00\x00\x00\x00\x00\x00", # [15]
    'pack'		=> "\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00", # [16]
    'parenthesis'	=> "\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00", # [32]
    'pipe'		=> "\x00\x00\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [10]
    'portable'		=> "\x00\x00\x00\x00\x08\x00\x00\x00\x00\x00\x00\x00", # [17]
    'precedence'	=> "\x00\x00\x00\x00\x00\x00\x00\x00\x08\x00\x00\x00", # [33]
    'printf'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x20\x00\x00\x00", # [34]
    'prototype'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x80\x00\x00\x00", # [35]
    'qw'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x00", # [36]
    'recursion'		=> "\x00\x00\x00\x00\x20\x00\x00\x00\x00\x00\x00\x00", # [18]
    'redefine'		=> "\x00\x00\x00\x00\x80\x00\x00\x00\x00\x00\x00\x00", # [19]
    'regexp'		=> "\x00\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00", # [20]
    'reserved'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08\x00\x00", # [37]
    'semicolon'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x20\x00\x00", # [38]
    'severe'		=> "\x00\x00\x00\x00\x00\xa8\x0a\x00\x00\x00\x00\x00", # [21..25]
    'signal'		=> "\x00\x00\x00\x00\x00\x00\x20\x00\x00\x00\x00\x00", # [26]
    'substr'		=> "\x00\x00\x00\x00\x00\x00\x80\x00\x00\x00\x00\x00", # [27]
    'syntax'		=> "\x00\x00\x00\x00\x00\x00\x00\xaa\xaa\x2a\x00\x00", # [28..38]
    'taint'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80\x00\x00", # [39]
    'threads'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00", # [40]
    'uninitialized'	=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08\x00", # [41]
    'unopened'		=> "\x00\x00\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [11]
    'unpack'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x20\x00", # [42]
    'untie'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80\x00", # [43]
    'utf8'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02", # [44]
    'void'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08", # [45]
    'y2k'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x20", # [46]
  );

$NONE     = "\0\0\0\0\0\0\0\0\0\0\0\0";
$LAST_BIT = 94 ;
$BYTES    = 12 ;

$All = "" ; vec($All, $Offsets{'all'}, 2) = 3 ;

sub Croaker
{
    delete $Carp::CarpInternal{'warnings'};
    Carp::croak(@_);
}

sub bits
{
    # called from B::Deparse.pm

    push @_, 'all' unless @_;

    my $mask;
    my $catmask ;
    my $fatal = 0 ;
    my $no_fatal = 0 ;

    foreach my $word ( @_ ) {
	if ($word eq 'FATAL') {
	    $fatal = 1;
	    $no_fatal = 0;
	}
	elsif ($word eq 'NONFATAL') {
	    $fatal = 0;
	    $no_fatal = 1;
	}
	elsif ($catmask = $Bits{$word}) {
	    $mask |= $catmask ;
	    $mask |= $DeadBits{$word} if $fatal ;
	    $mask &= ~($DeadBits{$word}|$All) if $no_fatal ;
	}
	else
          { Croaker("Unknown warnings category '$word'")}
    }

    return $mask ;
}

sub import 
{
    shift;

    my $catmask ;
    my $fatal = 0 ;
    my $no_fatal = 0 ;

    my $mask = ${^WARNING_BITS} ;

    if (vec($mask, $Offsets{'all'}, 1)) {
        $mask |= $Bits{'all'} ;
        $mask |= $DeadBits{'all'} if vec($mask, $Offsets{'all'}+1, 1);
    }
    
    push @_, 'all' unless @_;

    foreach my $word ( @_ ) {
	if ($word eq 'FATAL') {
	    $fatal = 1;
	    $no_fatal = 0;
	}
	elsif ($word eq 'NONFATAL') {
	    $fatal = 0;
	    $no_fatal = 1;
	}
	elsif ($catmask = $Bits{$word}) {
	    $mask |= $catmask ;
	    $mask |= $DeadBits{$word} if $fatal ;
	    $mask &= ~($DeadBits{$word}|$All) if $no_fatal ;
	}
	else
          { Croaker("Unknown warnings category '$word'")}
    }

    ${^WARNING_BITS} = $mask ;
}

sub unimport 
{
    shift;

    my $catmask ;
    my $mask = ${^WARNING_BITS} ;

    if (vec($mask, $Offsets{'all'}, 1)) {
        $mask |= $Bits{'all'} ;
        $mask |= $DeadBits{'all'} if vec($mask, $Offsets{'all'}+1, 1);
    }

    push @_, 'all' unless @_;

    foreach my $word ( @_ ) {
	if ($word eq 'FATAL') {
	    next; 
	}
	elsif ($catmask = $Bits{$word}) {
	    $mask &= ~($catmask | $DeadBits{$word} | $All);
	}
	else
          { Croaker("Unknown warnings category '$word'")}
    }

    ${^WARNING_BITS} = $mask ;
}

my %builtin_type; @builtin_type{qw(SCALAR ARRAY HASH CODE REF GLOB LVALUE Regexp)} = ();

sub __chk
{
    my $category ;
    my $offset ;
    my $isobj = 0 ;

    if (@_) {
        # check the category supplied.
        $category = shift ;
        if (my $type = ref $category) {
            Croaker("not an object")
                if exists $builtin_type{$type};
	    $category = $type;
            $isobj = 1 ;
        }
        $offset = $Offsets{$category};
        Croaker("Unknown warnings category '$category'")
	    unless defined $offset;
    }
    else {
        $category = (caller(1))[0] ;
        $offset = $Offsets{$category};
        Croaker("package '$category' not registered for warnings")
	    unless defined $offset ;
    }

    my $this_pkg = (caller(1))[0] ;
    my $i = 2 ;
    my $pkg ;

    if ($isobj) {
        while (do { { package DB; $pkg = (caller($i++))[0] } } ) {
            last unless @DB::args && $DB::args[0] =~ /^$category=/ ;
        }
	$i -= 2 ;
    }
    else {
        for ($i = 2 ; $pkg = (caller($i))[0] ; ++ $i) {
            last if $pkg ne $this_pkg ;
        }
        $i = 2
            if !$pkg || $pkg eq $this_pkg ;
    }

    my $callers_bitmask = (caller($i))[9] ;
    return ($callers_bitmask, $offset, $i) ;
}

sub enabled
{
    Croaker("Usage: warnings::enabled([category])")
	unless @_ == 1 || @_ == 0 ;

    my ($callers_bitmask, $offset, $i) = __chk(@_) ;

    return 0 unless defined $callers_bitmask ;
    return vec($callers_bitmask, $offset, 1) ||
           vec($callers_bitmask, $Offsets{'all'}, 1) ;
}


sub warn
{
    Croaker("Usage: warnings::warn([category,] 'message')")
	unless @_ == 2 || @_ == 1 ;

    my $message = pop ;
    my ($callers_bitmask, $offset, $i) = __chk(@_) ;
    Carp::croak($message)
	if vec($callers_bitmask, $offset+1, 1) ||
	   vec($callers_bitmask, $Offsets{'all'}+1, 1) ;
    Carp::carp($message) ;
}

sub warnif
{
    Croaker("Usage: warnings::warnif([category,] 'message')")
	unless @_ == 2 || @_ == 1 ;

    my $message = pop ;
    my ($callers_bitmask, $offset, $i) = __chk(@_) ;

    return
        unless defined $callers_bitmask &&
            	(vec($callers_bitmask, $offset, 1) ||
            	vec($callers_bitmask, $Offsets{'all'}, 1)) ;

    Carp::croak($message)
	if vec($callers_bitmask, $offset+1, 1) ||
	   vec($callers_bitmask, $Offsets{'all'}+1, 1) ;

    Carp::carp($message) ;
}

1;
