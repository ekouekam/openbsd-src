require Test::Simple;

push @INC, 't/lib';
require Test::Simple::Catch;
my($out, $err) = Test::Simple::Catch::caught();

Test::Simple->import(tests => 5);

require Dev::Null;
tie *STDERR, 'Dev::Null';

ok(1);
ok(1);
ok(1);
ok(1);
ok(1);

$! = 0;
die "This is a test";
