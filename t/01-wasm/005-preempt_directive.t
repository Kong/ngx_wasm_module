# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

our $nginxV = $t::TestWasm::nginxV;

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: preempt_timeout directive - 50ms
--- main_config
    wasm {
        preempt_timeout 50ms;
    }
--- error_log eval
qr/\[info\] .*? async preemption enabled with 50ms timeout/
--- no_error_log
[error]
[crit]

