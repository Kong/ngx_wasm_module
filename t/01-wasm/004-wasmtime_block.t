# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: wasmtime{} - empty block
--- skip_eval: 5: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {}
    }
--- no_error_log
[error]
[crit]
stub
stub



=== TEST 2: wasmtime{} - single flag directive
--- skip_eval: 5: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag debug_info on;
        }
    }
--- error_log eval
qr/\[wasm\] setting flag "debug_info=on"/
--- no_error_log
[error]
[crit]
stub



=== TEST 3: wasmtime{} - multiple flag directives
--- skip_eval: 5: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag debug_info on;
            flag consume_fuel on;
        }
    }
--- error_log eval
[qr/\[wasm\] setting flag "debug_info=on"/,
qr/\[wasm\] setting flag "consume_fuel=on"/]
--- no_error_log
[error]
[crit]
