#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
    require "./test.pl";
}

use Config;
no warnings 'once';

my $test = 1;
my $tests_needing_perlio = 17;
plan(12 + $tests_needing_perlio);
print "ok 1\n";

open(DUPOUT,">&STDOUT");
open(DUPERR,">&STDERR");

open(STDOUT,">Io.dup")  || die "Can't open stdout";
open(STDERR,">&STDOUT") || die "Can't open stderr";

select(STDERR); $| = 1;
select(STDOUT); $| = 1;

print STDOUT "ok 2\n";
print STDERR "ok 3\n";

# Since some systems don't have echo, we use Perl.
$echo = qq{$^X -le "print q(ok %d)"};

$cmd = sprintf $echo, 4;
print `$cmd`;

$cmd = sprintf "$echo 1>&2", 5;
$cmd = sprintf $echo, 5 if $^O eq 'MacOS';  # don't know if we can do this ...
print `$cmd`;

# KNOWN BUG system() does not honor STDOUT redirections on VMS.
if( $^O eq 'VMS' ) {
    print "not ok $_ # TODO system() not honoring STDOUT redirect on VMS\n"
      for 6..7;
}
else {
    system sprintf $echo, 6;
    if ($^O eq 'MacOS') {
        system sprintf $echo, 7;
    }
    else {
        system sprintf "$echo 1>&2", 7;
    }
}

close(STDOUT) or die "Could not close: $!";
close(STDERR) or die "Could not close: $!";

open(STDOUT,">&DUPOUT") or die "Could not open: $!";
open(STDERR,">&DUPERR") or die "Could not open: $!";

if (($^O eq 'MSWin32') || ($^O eq 'NetWare') || ($^O eq 'VMS')) { print `type Io.dup` }
elsif ($^O eq 'MacOS') { system 'catenate Io.dup' }
else                   { system 'cat Io.dup' }
unlink 'Io.dup';

print STDOUT "ok 8\n";

open(F,">&",1) or die "Cannot dup to numeric 1: $!";
print F "ok 9\n";
close(F);

open(F,">&",'1') or die "Cannot dup to string '1': $!";
print F "ok 10\n";
close(F);

open(F,">&=",1) or die "Cannot dup to numeric 1: $!";
print F "ok 11\n";
close(F);

if ($Config{useperlio}) {
    open(F,">&=",'1') or die "Cannot dup to string '1': $!";
    print F "ok 12\n";
    close(F);
} else {
    open(F, ">&DUPOUT") or die "Cannot dup stdout back: $!";
    print F "ok 12\n";
    close(F);
}

# To get STDOUT back.
open(F, ">&DUPOUT") or die "Cannot dup stdout back: $!";

curr_test(13);

SKIP: {
    skip("need perlio", $tests_needing_perlio) unless $Config{useperlio};
    
    ok(open(F, ">&", STDOUT));
    isnt(fileno(F), fileno(STDOUT));
    close F;

    ok(open(F, "<&=STDIN")) or _diag $!;
    is(fileno(F), fileno(STDIN));
    close F;

    ok(open(F, ">&=STDOUT"));
    is(fileno(F), fileno(STDOUT));
    close F;

    ok(open(F, ">&=STDERR"));
    is(fileno(F), fileno(STDERR));
    close F;

    open(G, ">dup$$") or die;
    my $g = fileno(G);

    ok(open(F, ">&=$g"));
    is(fileno(F), $g);
    close F;

    ok(open(F, ">&=G"));
    is(fileno(F), $g);

    print G "ggg\n";
    print F "fff\n";

    close G; # flush first
    close F; # flush second

    open(G, "<dup$$") or die;
    {
	my $line;
	$line = <G>; chomp $line; is($line, "ggg");
	$line = <G>; chomp $line; is($line, "fff");
    }
    close G;

    open UTFOUT, '>:utf8', "dup$$" or die $!;
    open UTFDUP, '>&UTFOUT' or die $!;
    # some old greek saying.
    my $message = "\x{03A0}\x{0391}\x{039D}\x{03A4}\x{0391} \x{03A1}\x{0395}\x{0399}\n";
    print UTFOUT $message;
    print UTFDUP $message;
    binmode UTFDUP, ':utf8';
    print UTFDUP $message;
    close UTFOUT;
    close UTFDUP;
    open(UTFIN, "<:utf8", "dup$$") or die $!;
    {
	my $line;
	$line = <UTFIN>; is($line, $message);
	$line = <UTFIN>; is($line, $message);
	$line = <UTFIN>; is($line, $message);
    }
    close UTFIN;

    END { 1 while unlink "dup$$" }
}
