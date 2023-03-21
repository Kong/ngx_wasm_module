# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: bench - proxy_wasm on_phases filter
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        return 200;
    }
--- response_body
--- no_error_log
[error]
[crit]
