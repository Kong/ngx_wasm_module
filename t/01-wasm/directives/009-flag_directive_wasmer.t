# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

our $nginxV = $t::TestWasm::nginxV;

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: flag directive - wasmer - bad boolean value
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_bulk_memory no;
        }
    }
--- error_log eval
qr/\[emerg\] .*? failed setting wasmer flag: invalid value "no"/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 2: flag directive - wasmer - wasm_bulk_memory - on
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_bulk_memory on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_bulk_memory=on"/
--- no_error_log
[error]
[crit]



=== TEST 3: flag directive - wasmer - wasm_bulk_memory - off
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_bulk_memory off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_bulk_memory=off"/
--- no_error_log
[error]
[crit]



=== TEST 4: flag directive - wasmer - wasm_memory64 - on
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_memory64 on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_memory64=on"/
--- no_error_log
[error]
[crit]



=== TEST 5: flag directive - wasmer - wasm_memory64 - off
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_memory64 off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_memory64=off"/
--- no_error_log
[error]
[crit]



=== TEST 6: flag directive - wasmer - wasm_module_linking - on
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_module_linking on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_module_linking=on"/
--- no_error_log
[error]
[crit]



=== TEST 7: flag directive - wasmer - wasm_module_linking - off
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_module_linking off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_module_linking=off"/
--- no_error_log
[error]
[crit]



=== TEST 8: flag directive - wasmer - wasm_multi_memory - on
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_multi_memory on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_multi_memory=on"/
--- no_error_log
[error]
[crit]



=== TEST 9: flag directive - wasmer - wasm_multi_memory - off
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_multi_memory off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_multi_memory=off"/
--- no_error_log
[error]
[crit]



=== TEST 10: flag directive - wasmer - wasm_multi_value - on
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_multi_value on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_multi_value=on"/
--- no_error_log
[error]
[crit]



=== TEST 11: flag directive - wasmer - wasm_multi_value - off
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_multi_value off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_multi_value=off"/
--- no_error_log
[error]
[crit]



=== TEST 12: flag directive - wasmer - wasm_reference_types - on
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_reference_types on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_reference_types=on"/
--- no_error_log
[error]
[crit]



=== TEST 13: flag directive - wasmer - wasm_reference_types - off
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_reference_types off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_reference_types=off"/
--- no_error_log
[error]
[crit]



=== TEST 14: flag directive - wasmer - wasm_simd - on
--- skip_eval: 4: $::nginxV !~ m/wasmer/
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



=== TEST 15: flag directive - wasmer - wasm_simd - off
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_simd off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_simd=off"/
--- no_error_log
[error]
[crit]



=== TEST 16: flag directive - wasmer - wasm_tail_call - on
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_tail_call on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_tail_call=on"/
--- no_error_log
[error]
[crit]



=== TEST 17: flag directive - wasmer - wasm_tail_call - off
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_tail_call off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_tail_call=off"/
--- no_error_log
[error]
[crit]



=== TEST 18: flag directive - wasmer - wasm_threads - on
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_threads on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_threads=on"/
--- no_error_log
[error]
[crit]



=== TEST 19: flag directive - wasmer - wasm_threads - off
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        wasmer {
            flag wasm_threads off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_threads=off"/
--- no_error_log
[error]
[crit]
