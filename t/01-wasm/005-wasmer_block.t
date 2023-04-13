# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: wasmer{} - empty block
--- skip_eval: 5: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {}
    }
--- no_error_log
[error]
[crit]
stub
stub



=== TEST 2: wasmer{} - single flag directive
--- skip_eval: 5: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_simd on;
        }
    }
--- error_log eval
qr/\[wasm\] setting flag "wasm_simd=on"/
--- no_error_log
[error]
[crit]
stub



=== TEST 3: wasmer{} - multiple flag directives
--- skip_eval: 5: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_simd on;
            flag wasm_threads on;
        }
    }
--- error_log eval
[qr/\[wasm\] setting flag "wasm_simd=on"/,
qr/\[wasm\] setting flag "wasm_threads=on"/]
--- no_error_log
[error]
[crit]
