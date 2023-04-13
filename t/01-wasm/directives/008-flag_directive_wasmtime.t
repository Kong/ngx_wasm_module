# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

our $nginxV = $t::TestWasm::nginxV;

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: flag directive - wasmtime - bad size
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag static_memory_maximum_size 10U;
        }
    }
--- error_log eval
qr/\[emerg\] .*? failed setting wasmtime flag: invalid value "10U"/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 2: flag directive - wasmtime - bad boolean value
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag debug_info no;
        }
    }
--- error_log eval
qr/\[emerg\] .*? failed setting wasmtime flag: invalid value "no"/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 3: flag directive - wasmtime - debug_info - on
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
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



=== TEST 4: flag directive - wasmtime - debug_info - off
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag debug_info off;
        }
    }
--- error_log eval
qr/setting flag: "debug_info=off"/
--- no_error_log
[error]
[crit]



=== TEST 5: flag directive - wasmtime - consume_fuel - on
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag consume_fuel on;
        }
    }
--- error_log eval
qr/setting flag: "consume_fuel=on"/
--- no_error_log
[error]
[crit]



=== TEST 6: flag directive - wasmtime - consume_fuel - off
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag consume_fuel off;
        }
    }
--- error_log eval
qr/setting flag: "consume_fuel=off"/
--- no_error_log
[error]
[crit]



=== TEST 7: flag directive - wasmtime - epoch_interruption - on
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag epoch_interruption on;
        }
    }
--- error_log eval
qr/setting flag: "epoch_interruption=on"/
--- no_error_log
[error]
[crit]



=== TEST 8: flag directive - wasmtime - epoch_interruption - off
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag epoch_interruption off;
        }
    }
--- error_log eval
qr/setting flag: "epoch_interruption=off"/
--- no_error_log
[error]
[crit]



=== TEST 9: flag directive - wasmtime - max_wasm_stack
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag max_wasm_stack 2m;
        }
    }
--- error_log eval
qr/setting flag: "max_wasm_stack=2m"/
--- no_error_log
[error]
[crit]



=== TEST 10: flag directive - wasmtime - wasm_threads - on
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag wasm_threads on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_threads=on"/
--- no_error_log
[error]
[crit]



=== TEST 11: flag directive - wasmtime - wasm_threads - off
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag wasm_threads off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_threads=off"/
--- no_error_log
[error]
[crit]



=== TEST 12: flag directive - wasmtime - wasm_reference_types - on
--- SKIP: Unsupported (hard-coded)
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag wasm_reference_types on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_reference_types=on"/
--- no_error_log
[error]
[crit]



=== TEST 13: flag directive - wasmtime - wasm_reference_types - off
--- SKIP: Unsupported (hard-coded)
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag wasm_reference_types off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_reference_types=off"/
--- no_error_log
[error]
[crit]



=== TEST 14: flag directive - wasmtime - wasm_simd - on
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag wasm_simd on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_simd=on"/
--- no_error_log
[error]
[crit]



=== TEST 15: flag directive - wasmtime - wasm_simd - off
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag wasm_simd off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_simd=off"/
--- no_error_log
[error]
[crit]



=== TEST 16: flag directive - wasmtime - wasm_bulk_memory - on
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag wasm_bulk_memory on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_bulk_memory=on"/
--- no_error_log
[error]
[crit]



=== TEST 17: flag directive - wasmtime - wasm_bulk_memory - off
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag wasm_reference_types off;
            flag wasm_bulk_memory off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_bulk_memory=off"/
--- no_error_log
[error]
[crit]



=== TEST 18: flag directive - wasmtime - wasm_multi_value - on
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag wasm_multi_value on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_multi_value=on"/
--- no_error_log
[error]
[crit]



=== TEST 19: flag directive - wasmtime - wasm_multi_value - off
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag wasm_multi_value off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_multi_value=off"/
--- no_error_log
[error]
[crit]



=== TEST 20: flag directive - wasmtime - wasm_multi_memory - on
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag wasm_multi_memory on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_multi_memory=on"/
--- no_error_log
[error]
[crit]



=== TEST 21: flag directive - wasmtime - wasm_multi_memory - off
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag wasm_multi_memory off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_multi_memory=off"/
--- no_error_log
[error]
[crit]



=== TEST 22: flag directive - wasmtime - wasm_memory64 - on
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag wasm_memory64 on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_memory64=on"/
--- no_error_log
[error]
[crit]



=== TEST 23: flag directive - wasmtime - wasm_memory64 - off
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag wasm_memory64 off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_memory64=off"/
--- no_error_log
[error]
[crit]



=== TEST 24: flag directive - wasmtime - strategy - auto
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag strategy auto;
        }
    }
--- error_log eval
qr/setting flag: "strategy=auto"/
--- no_error_log
[error]
[crit]



=== TEST 25: flag directive - wasmtime - strategy - cranelift
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag strategy cranelift;
        }
    }
--- error_log eval
qr/setting flag: "strategy=cranelift"/
--- no_error_log
[error]
[crit]



=== TEST 26: flag directive - wasmtime - parallel_compilation - on
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag parallel_compilation on;
        }
    }
--- error_log eval
qr/setting flag: "parallel_compilation=on"/
--- no_error_log
[error]
[crit]



=== TEST 27: flag directive - wasmtime - parallel_compilation - off
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag parallel_compilation off;
        }
    }
--- error_log eval
qr/setting flag: "parallel_compilation=off"/
--- no_error_log
[error]
[crit]



=== TEST 28: flag directive - wasmtime - cranelift_debug_verifier - on
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag cranelift_debug_verifier on;
        }
    }
--- error_log eval
qr/setting flag: "cranelift_debug_verifier=on"/
--- no_error_log
[error]
[crit]



=== TEST 29: flag directive - wasmtime - cranelift_debug_verifier - off
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag cranelift_debug_verifier off;
        }
    }
--- error_log eval
qr/setting flag: "cranelift_debug_verifier=off"/
--- no_error_log
[error]
[crit]



=== TEST 30: flag directive - wasmtime - cranelift_nan_canonicalization - on
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag cranelift_nan_canonicalization on;
        }
    }
--- error_log eval
qr/setting flag: "cranelift_nan_canonicalization=on"/
--- no_error_log
[error]
[crit]



=== TEST 31: flag directive - wasmtime - cranelift_nan_canonicalization - off
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag cranelift_nan_canonicalization off;
        }
    }
--- error_log eval
qr/setting flag: "cranelift_nan_canonicalization=off"/
--- no_error_log
[error]
[crit]



=== TEST 32: flag directive - wasmtime - cranelift_opt_level - none
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag cranelift_opt_level none;
        }
    }
--- error_log eval
qr/setting flag: "cranelift_opt_level=none"/
--- no_error_log
[error]
[crit]



=== TEST 33: flag directive - wasmtime - cranelift_opt_level - speed
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag cranelift_opt_level speed;
        }
    }
--- error_log eval
qr/setting flag: "cranelift_opt_level=speed"/
--- no_error_log
[error]
[crit]



=== TEST 34: flag directive - wasmtime - cranelift_opt_level - speed_and_size
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag cranelift_opt_level speed_and_size;
        }
    }
--- error_log eval
qr/setting flag: "cranelift_opt_level=speed_and_size"/
--- no_error_log
[error]
[crit]



=== TEST 35: flag directive - wasmtime - profiler - none
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag profiler none;
        }
    }
--- error_log eval
qr/setting flag: "profiler=none"/
--- no_error_log
[error]
[crit]



=== TEST 36: flag directive - wasmtime - profiler - jitdump
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag profiler jitdump;
        }
    }
--- error_log eval
qr/setting flag: "profiler=jitdump"/
--- no_error_log
[error]
[crit]



=== TEST 37: flag directive - wasmtime - profiler - vtune
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag profiler vtune;
        }
    }
--- error_log eval
qr/setting flag: "profiler=vtune"/
--- no_error_log
[error]
[crit]



=== TEST 38: flag directive - wasmtime - profiler - perfmap
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag profiler perfmap;
        }
    }
--- error_log eval
qr/setting flag: "profiler=perfmap"/
--- no_error_log
[error]
[crit]



=== TEST 39: flag directive - wasmtime - static_memory_maximum_size
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag static_memory_maximum_size 10m;
        }
    }
--- error_log eval
qr/setting flag: "static_memory_maximum_size=10m"/
--- no_error_log
[error]
[crit]



=== TEST 40: flag directive - wasmtime - static_memory_guard_size
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag static_memory_guard_size 4000m;
        }
    }
--- error_log eval
qr/setting flag: "static_memory_guard_size=4000m"/
--- no_error_log
[error]
[crit]



=== TEST 41: flag directive - wasmtime - dynamic_memory_guard_size
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        wasmtime {
            flag dynamic_memory_guard_size 64k;
        }
    }
--- error_log eval
qr/setting flag: "dynamic_memory_guard_size=64k"/
--- no_error_log
[error]
[crit]
