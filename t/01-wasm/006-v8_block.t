# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 5);

run_tests();

__DATA__

=== TEST 1: v8{} - empty block
--- skip_eval: 5: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {}
    }
--- no_error_log
[error]
[crit]
stub
stub



=== TEST 2: v8{} - single flag directive
--- skip_eval: 5: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag turbo_stats_wasm on;
        }
    }
--- error_log eval
qr/\[wasm\] setting flag "turbo_stats_wasm=on"/
--- no_error_log
[error]
[crit]
stub



=== TEST 3: v8{} - multiple flag directives
--- skip_eval: 5: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag turbo_stats_wasm on;
            flag expose_wasm on;
        }
    }
--- error_log eval
[qr/\[wasm\] setting flag "turbo_stats_wasm=on"/,
qr/\[wasm\] setting flag "expose_wasm=on"/]
--- no_error_log
[error]
[crit]
