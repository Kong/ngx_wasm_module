# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

our $nginxV = $t::TestWasm::nginxV;
our $validFlagsConf = "
    wasm {
        wasmtime {
            flag static_memory_maximum_size 10m;
        }

        wasmer {
            flag wasm_bulk_memory on;
        }

        v8 {
            flag turbo_stats_wasm on;
        }
    }
";

our $unknownFlagsConf = "
    wasm {
        wasmtime {
            flag non_existent_flag on;
        }

        wasmer {
            flag non_existent_flag on;
        }

        v8 {
            flag non_existent_flag on;
        }
    }
";

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: flag directive - wasmtime block - valid flag
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config eval
$::validFlagsConf
--- error_log eval
qr/\[wasm\] setting flag "static_memory_maximum_size=10m"/
--- no_error_log
[error]
[crit]



=== TEST 2: flag directive - wasmer block - valid flag
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config eval
$::validFlagsConf
--- error_log eval
qr/\[wasm\] setting flag "wasm_bulk_memory=on"/
--- no_error_log
[error]
[crit]



=== TEST 3: flag directive - v8 block - valid flag
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config eval
$::validFlagsConf
--- error_log eval
qr/\[wasm\] setting flag "turbo_stats_wasm=on"/
--- no_error_log
[error]
[crit]



=== TEST 4: flag directive - wasmer block - unsupported flag
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag max_wasm_stack 100k;
        }
    }
--- error_log eval
qr/\[wasm\] unsupported "wasmer" configuration flag: "max_wasm_stack"/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 5: flag directive - wasmtime block - unknown flag
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config eval
$::unknownFlagsConf
--- error_log eval
qr/\[wasm\] unknown "wasmtime" configuration flag: "non_existent_flag"/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 6: flag directive - wasmer block - unknown flag
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config eval
$::unknownFlagsConf
--- error_log eval
qr/\[wasm\] unknown "wasmer" configuration flag: "non_existent_flag"/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 7: flag directive - v8 block - unknown flag
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config eval
$::unknownFlagsConf
--- error_log eval
qr/\[wasm\] unknown "v8" configuration flag: "non_existent_flag"/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 8: flag directive - duplicated flag
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag static_memory_maximum_size 10k;
            flag static_memory_maximum_size 100k;
        }
    }
--- grep_error_log eval: qr/setting flag.*/
--- grep_error_log_out eval
qr/setting flag "static_memory_maximum_size=10k".*?
setting flag "static_memory_maximum_size=100k".*?/
--- no_error_log
[error]
[crit]
