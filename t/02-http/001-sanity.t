# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(4);
run_tests();

__DATA__

=== TEST 1:
--- valgrind
--- config
    location /t {
        root html;
    }
--- error_code: 404
--- response_body_like: 404 Not Found
--- error_log eval
qr/\[error\] .*? No such file or directory/
--- no_error_log
[crit]
