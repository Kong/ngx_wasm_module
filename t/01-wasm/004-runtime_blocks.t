# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(5);
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
[alert]
[emerg]



=== TEST 2: wasmtime{} - single flag directive
--- skip_eval: 5: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag debug_info on;
        }
    }
--- error_log eval
qr/setting flag: "debug_info=on"/
--- no_error_log
[error]
[crit]
[alert]



=== TEST 3: wasmtime{} - multiple flag directives
--- valgrind
--- skip_eval: 5: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag debug_info on;
            flag consume_fuel on;
        }
    }
--- error_log eval
[qr/setting flag: "debug_info=on"/,
qr/setting flag: "consume_fuel=on"/]
--- no_error_log
[error]
[crit]



=== TEST 4: wasmer{} - empty block
--- skip_eval: 5: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {}
    }
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 5: wasmer{} - single flag directive
--- skip_eval: 5: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_simd on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_simd=on"/
--- no_error_log
[error]
[crit]
[alert]



=== TEST 6: wasmer{} - multiple flag directives
--- skip_eval: 5: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_simd on;
            flag wasm_threads on;
        }
    }
--- error_log eval
[qr/setting flag: "wasm_simd=on"/,
qr/setting flag: "wasm_threads=on"/]
--- no_error_log
[error]
[crit]



=== TEST 7: v8{} - empty block
--- skip_eval: 5: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {}
    }
--- no_error_log
[error]
[crit]
[alert]
[emerg]



=== TEST 8: v8{} - single flag directive
--- skip_eval: 5: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag turbo_stats_wasm on;
        }
    }
--- error_log eval
qr/setting flag: "turbo_stats_wasm=on"/
--- no_error_log
[error]
[crit]
[alert]



=== TEST 9: v8{} - multiple flag directives
--- skip_eval: 5: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag turbo_stats_wasm on;
            flag expose_wasm on;
        }
    }
--- error_log eval
[qr/setting flag: "turbo_stats_wasm=on"/,
qr/setting flag: "expose_wasm=on"/]
--- no_error_log
[error]
[crit]
