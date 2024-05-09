# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: metrics{} - empty block
--- valgrind
--- main_config
    wasm {
        metrics {}
    }
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 2: metrics{} - duplicated block
--- main_config
    wasm {
        metrics {}
        metrics {}
    }
--- error_log: is duplicate
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 3: metrics{} - bad context
--- http_config
    metrics {}
--- error_log: is not allowed here
--- no_error_log
[error]
[crit]
--- must_die
