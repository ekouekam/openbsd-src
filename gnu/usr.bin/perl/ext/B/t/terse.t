#!./perl

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
}

use Test::More tests => 15;

use_ok( 'B::Terse' );

# indent should return a string indented four spaces times the argument
is( B::Terse::indent(2), ' ' x 8, 'indent with an argument' );
is( B::Terse::indent(), '', 'indent with no argument' );

# this should fail without a reference
eval { B::Terse::terse('scalar') };
like( $@, qr/not a reference/, 'terse() fed bad parameters' );

# now point it at a sub and see what happens
sub foo {}

my $sub;
eval{ $sub = B::Terse::compile('', 'foo') };
is( $@, '', 'compile()' );
ok( defined &$sub, 'valid subref back from compile()' );

# and point it at a real sub and hope the returned ops look alright
my $out = tie *STDOUT, 'TieOut';
$sub = B::Terse::compile('', 'bar');
$sub->();

# now build some regexes that should match the dumped ops
my ($hex, $op) = ('\(0x[a-f0-9]+\)', '\s+\w+');
my %ops = map { $_ => qr/$_ $hex$op/ }
	qw ( OP	COP	LOOP PMOP UNOP BINOP LOGOP LISTOP );

# split up the output lines into individual ops (terse is, well, terse!)
# use an array here so $_ is modifiable
my @lines = split(/\n+/, $out->read);
foreach (@lines) {
	next unless /\S/;
	s/^\s+//;
	if (/^([A-Z]+)\s+/) {
		my $op = $1;
		next unless exists $ops{$op};
		like( $_, $ops{$op}, "$op " );
		delete $ops{$op};
		s/$ops{$op}//;
		redo if $_;
	}
}

warn "# didn't find " . join(' ', keys %ops) if keys %ops;

# XXX:
# this tries to get at all tersified optypes in B::Terse
# if you add AV, NULL, PADOP, PVOP, or SPECIAL, add it to the regex above too
#
use vars qw( $a $b );
sub bar {
	# OP SVOP COP IV here or in sub definition
	my @bar = (1, 2, 3);

	# got a GV here
	my $foo = $a + $b;

	# NV here
	$a = 1.234;

	# this is awful, but it gives a PMOP
	my $boo = split('', $foo);

	# PMOP
	LOOP: for (1 .. 10) {
		last LOOP if $_ % 2;
	}

	# make a PV
	$foo = "a string";

	# make an OP_SUBSTCONT
	$foo =~ s/(a)/$1/;
}

SKIP: {
    use Config;
    skip("- B::Terse won't grok RVs under ithreads yet", 1)
	if $Config{useithreads};
    # Schwern's example of finding an RV
    my $path = join " ", map { qq["-I$_"] } @INC;
    $path = '-I::lib -MMac::err=unix' if $^O eq 'MacOS';
    my $redir = $^O eq 'MacOS' ? '' : "2>&1";
    my $items = qx{$^X $path "-MO=Terse" -le "print \\42" $redir};
    like( $items, qr/RV $hex \\42/, 'RV' );
}

package TieOut;

sub TIEHANDLE {
	bless( \(my $out), $_[0] );
}

sub PRINT {
	my $self = shift;
	$$self .= join('', @_);
}

sub PRINTF {
	my $self = shift;
	$$self .= sprintf(@_);
}

sub read {
	my $self = shift;
	return substr($$self, 0, length($$self), '');
}
