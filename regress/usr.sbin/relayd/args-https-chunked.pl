# test chunked https 1.1 connection over http relay

use strict;
use warnings;

my @lengths = ([ 251, 10000, 10 ], 1, [2, 3]);
our %args = (
    client => {
	func => \&http_client,
	lengths => \@lengths,
	ssl => 1,
    },
    relayd => {
	protocol => [ "http",
	    "request header log foo",
	    "response header log Transfer-Encoding",
	],
	loggrep => { "log 'Transfer-Encoding: chunked'" => 2 },
	forwardssl => 1,
	listenssl => 1,
    },
    server => {
	func => \&http_server,
	ssl => 1,
    },
    lengths => \@lengths,
    md5 => "bc3a3f39af35fe5b1687903da2b00c7f",
);

1;
