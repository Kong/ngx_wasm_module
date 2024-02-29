# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

our $nginxV = $t::TestWasmX::nginxV;

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: flag directive - v8 - args list too long (too many flags)
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config eval
my $main = "
    wasm {
        v8 {
";

    for (1..500) { $main .= "\t\t\tflag turbo_stats_wasm on;\n"; }

$main .= "
        }
    }
";

$main
--- error_log eval
qr/\[emerg\] .*? failed setting v8 flag: args list too long \(512 bytes max\) "--turbo-stats-wasm/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 2: flag directive - v8 - bad boolean value
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag turbo_stats_wasm no;
        }
    }
--- error_log eval
qr/\[emerg\] .*? failed setting v8 flag: invalid value "no"/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 3: flag directive - v8 - turbo_stats_wasm - on
--- skip_eval: 4: $::nginxV !~ m/v8/
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



=== TEST 4: flag directive - v8 - turbo_stats_wasm - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag turbo_stats_wasm off;
        }
    }
--- error_log eval
qr/setting flag: "turbo_stats_wasm=off"/
--- no_error_log
[error]
[crit]



=== TEST 5: flag directive - v8 - turbo_inline_js_wasm_calls - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag turbo_inline_js_wasm_calls on;
        }
    }
--- error_log eval
qr/setting flag: "turbo_inline_js_wasm_calls=on"/
--- no_error_log
[error]
[crit]



=== TEST 6: flag directive - v8 - turbo_inline_js_wasm_calls - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag turbo_inline_js_wasm_calls off;
        }
    }
--- error_log eval
qr/setting flag: "turbo_inline_js_wasm_calls=off"/
--- no_error_log
[error]
[crit]



=== TEST 7: flag directive - v8 - wasm_generic_wrapper - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_generic_wrapper on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_generic_wrapper=on"/
--- no_error_log
[error]
[crit]



=== TEST 8: flag directive - v8 - wasm_generic_wrapper - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_generic_wrapper off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_generic_wrapper=off"/
--- no_error_log
[error]
[crit]



=== TEST 9: flag directive - v8 - expose_wasm - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag expose_wasm on;
        }
    }
--- error_log eval
qr/setting flag: "expose_wasm=on"/
--- no_error_log
[error]
[crit]



=== TEST 10: flag directive - v8 - expose_wasm - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag expose_wasm off;
        }
    }
--- error_log eval
qr/setting flag: "expose_wasm=off"/
--- no_error_log
[error]
[crit]



=== TEST 11: flag directive - v8 - wasm_num_compilation_tasks
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_num_compilation_tasks 128;
        }
    }
--- error_log eval
qr/setting flag: "wasm_num_compilation_tasks=128"/
--- no_error_log
[error]
[crit]



=== TEST 12: flag directive - v8 - wasm_write_protect_code_memory - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_write_protect_code_memory on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_write_protect_code_memory=on"/
--- no_error_log
[error]
[crit]



=== TEST 13: flag directive - v8 - wasm_write_protect_code_memory - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_write_protect_code_memory off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_write_protect_code_memory=off"/
--- no_error_log
[error]
[crit]



=== TEST 14: flag directive - v8 - wasm_memory_protection_keys - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            # wasm_memory_protection_keys off makes nginx core dump
        # }
        flag wasm_memory_protection_keys on;
    }
--- error_log eval
qr/setting flag: "wasm_memory_protection_keys=on"/
--- no_error_log
[error]
[crit]



=== TEST 15: flag directive - v8 - wasm_async_compilation - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_async_compilation on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_async_compilation=on"/
--- no_error_log
[error]
[crit]



=== TEST 16: flag directive - v8 - wasm_async_compilation - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_async_compilation off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_async_compilation=off"/
--- no_error_log
[error]
[crit]



=== TEST 17: flag directive - v8 - wasm_test_streaming - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_test_streaming on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_test_streaming=on"/
--- no_error_log
[error]
[crit]



=== TEST 18: flag directive - v8 - wasm_test_streaming - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_test_streaming off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_test_streaming=off"/
--- no_error_log
[error]
[crit]



=== TEST 19: flag directive - v8 - wasm_tier_up - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_tier_up on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_tier_up=on"/
--- no_error_log
[error]
[crit]



=== TEST 20: flag directive - v8 - wasm_tier_up - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_tier_up off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_tier_up=off"/
--- no_error_log
[error]
[crit]



=== TEST 21: flag directive - v8 - wasm_dynamic_tiering - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_dynamic_tiering on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_dynamic_tiering=on"/
--- no_error_log
[error]
[crit]



=== TEST 22: flag directive - v8 - wasm_dynamic_tiering - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_dynamic_tiering off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_dynamic_tiering=off"/
--- no_error_log
[error]
[crit]



=== TEST 23: flag directive - v8 - trace_wasm_compilation_times - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag trace_wasm_compilation_times on;
        }
    }
--- error_log eval
qr/setting flag: "trace_wasm_compilation_times=on"/
--- no_error_log
[error]
[crit]



=== TEST 24: flag directive - v8 - trace_wasm_compilation_times - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag trace_wasm_compilation_times off;
        }
    }
--- error_log eval
qr/setting flag: "trace_wasm_compilation_times=off"/
--- no_error_log
[error]
[crit]



=== TEST 25: flag directive - v8 - trace_wasm_memory - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag trace_wasm_memory on;
        }
    }
--- error_log eval
qr/setting flag: "trace_wasm_memory=on"/
--- no_error_log
[error]
[crit]



=== TEST 26: flag directive - v8 - trace_wasm_memory - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag trace_wasm_memory off;
        }
    }
--- error_log eval
qr/setting flag: "trace_wasm_memory=off"/
--- no_error_log
[error]
[crit]



=== TEST 27: flag directive - v8 - experimental_wasm_compilation_hints - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_compilation_hints on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_compilation_hints=on"/
--- no_error_log
[error]
[crit]



=== TEST 28: flag directive - v8 - experimental_wasm_compilation_hints - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_compilation_hints off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_compilation_hints=off"/
--- no_error_log
[error]
[crit]



=== TEST 29: flag directive - v8 - experimental_wasm_gc - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_gc on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_gc=on"/
--- no_error_log
[error]
[crit]



=== TEST 30: flag directive - v8 - experimental_wasm_gc - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_gc off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_gc=off"/
--- no_error_log
[error]
[crit]



=== TEST 31: flag directive - v8 - experimental_wasm_nn_locals - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_nn_locals on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_nn_locals=on"/
--- no_error_log
[error]
[crit]



=== TEST 32: flag directive - v8 - experimental_wasm_nn_locals - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_nn_locals off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_nn_locals=off"/
--- no_error_log
[error]
[crit]



=== TEST 33: flag directive - v8 - experimental_wasm_unsafe_nn_locals - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_unsafe_nn_locals on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_unsafe_nn_locals=on"/
--- no_error_log
[error]
[crit]



=== TEST 34: flag directive - v8 - experimental_wasm_unsafe_nn_locals - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_unsafe_nn_locals off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_unsafe_nn_locals=off"/
--- no_error_log
[error]
[crit]



=== TEST 35: flag directive - v8 - experimental_wasm_assume_ref_cast_succeeds - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_assume_ref_cast_succeeds on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_assume_ref_cast_succeeds=on"/
--- no_error_log
[error]
[crit]



=== TEST 36: flag directive - v8 - experimental_wasm_assume_ref_cast_succeeds - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_assume_ref_cast_succeeds off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_assume_ref_cast_succeeds=off"/
--- no_error_log
[error]
[crit]



=== TEST 37: flag directive - v8 - experimental_wasm_skip_null_checks - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_skip_null_checks on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_skip_null_checks=on"/
--- no_error_log
[error]
[crit]



=== TEST 38: flag directive - v8 - experimental_wasm_skip_null_checks - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_skip_null_checks off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_skip_null_checks=off"/
--- no_error_log
[error]
[crit]



=== TEST 39: flag directive - v8 - experimental_wasm_skip_bounds_checks - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_skip_bounds_checks on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_skip_bounds_checks=on"/
--- no_error_log
[error]
[crit]



=== TEST 40: flag directive - v8 - experimental_wasm_skip_bounds_checks - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_skip_bounds_checks off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_skip_bounds_checks=off"/
--- no_error_log
[error]
[crit]



=== TEST 41: flag directive - v8 - experimental_wasm_typed_funcref - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_typed_funcref on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_typed_funcref=on"/
--- no_error_log
[error]
[crit]



=== TEST 42: flag directive - v8 - experimental_wasm_typed_funcref - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_typed_funcref off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_typed_funcref=off"/
--- no_error_log
[error]
[crit]



=== TEST 43: flag directive - v8 - experimental_wasm_memory64 - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_memory64 on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_memory64=on"/
--- no_error_log
[error]
[crit]



=== TEST 44: flag directive - v8 - experimental_wasm_memory64 - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_memory64 off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_memory64=off"/
--- no_error_log
[error]
[crit]



=== TEST 45: flag directive - v8 - experimental_wasm_relaxed_simd - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_relaxed_simd on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_relaxed_simd=on"/
--- no_error_log
[error]
[crit]



=== TEST 46: flag directive - v8 - experimental_wasm_relaxed_simd - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_relaxed_simd off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_relaxed_simd=off"/
--- no_error_log
[error]
[crit]



=== TEST 47: flag directive - v8 - experimental_wasm_branch_hinting - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_branch_hinting on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_branch_hinting=on"/
--- no_error_log
[error]
[crit]



=== TEST 48: flag directive - v8 - experimental_wasm_branch_hinting - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_branch_hinting off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_branch_hinting=off"/
--- no_error_log
[error]
[crit]



=== TEST 49: flag directive - v8 - experimental_wasm_stack_switching - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_stack_switching on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_stack_switching=on"/
--- no_error_log
[error]
[crit]



=== TEST 50: flag directive - v8 - experimental_wasm_stack_switching - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_stack_switching off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_stack_switching=off"/
--- no_error_log
[error]
[crit]



=== TEST 51: flag directive - v8 - experimental_wasm_extended_const - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_extended_const on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_extended_const=on"/
--- no_error_log
[error]
[crit]



=== TEST 52: flag directive - v8 - experimental_wasm_extended_const - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_extended_const off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_extended_const=off"/
--- no_error_log
[error]
[crit]



=== TEST 53: flag directive - v8 - experimental_wasm_return_call - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_return_call on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_return_call=on"/
--- no_error_log
[error]
[crit]



=== TEST 54: flag directive - v8 - experimental_wasm_return_call - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_return_call off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_return_call=off"/
--- no_error_log
[error]
[crit]



=== TEST 55: flag directive - v8 - experimental_wasm_type_reflection - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_type_reflection on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_type_reflection=on"/
--- no_error_log
[error]
[crit]



=== TEST 56: flag directive - v8 - experimental_wasm_type_reflection - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_type_reflection off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_type_reflection=off"/
--- no_error_log
[error]
[crit]



=== TEST 57: flag directive - v8 - experimental_wasm_simd - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_simd on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_simd=on"/
--- no_error_log
[error]
[crit]



=== TEST 58: flag directive - v8 - experimental_wasm_simd - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_simd off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_simd=off"/
--- no_error_log
[error]
[crit]



=== TEST 59: flag directive - v8 - experimental_wasm_threads - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_threads on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_threads=on"/
--- no_error_log
[error]
[crit]



=== TEST 60: flag directive - v8 - experimental_wasm_threads - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_threads off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_threads=off"/
--- no_error_log
[error]
[crit]



=== TEST 61: flag directive - v8 - experimental_wasm_eh - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_eh on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_eh=on"/
--- no_error_log
[error]
[crit]



=== TEST 62: flag directive - v8 - experimental_wasm_eh - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_eh off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_eh=off"/
--- no_error_log
[error]
[crit]



=== TEST 63: flag directive - v8 - wasm_gc_js_interop - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_gc_js_interop on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_gc_js_interop=on"/
--- no_error_log
[error]
[crit]



=== TEST 64: flag directive - v8 - wasm_gc_js_interop - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_gc_js_interop off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_gc_js_interop=off"/
--- no_error_log
[error]
[crit]



=== TEST 65: flag directive - v8 - wasm_staging - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_staging on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_staging=on"/
--- no_error_log
[error]
[crit]



=== TEST 66: flag directive - v8 - wasm_staging - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_staging off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_staging=off"/
--- no_error_log
[error]
[crit]



=== TEST 67: flag directive - v8 - wasm_opt - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_opt on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_opt=on"/
--- no_error_log
[error]
[crit]



=== TEST 68: flag directive - v8 - wasm_opt - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_opt off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_opt=off"/
--- no_error_log
[error]
[crit]



=== TEST 69: flag directive - v8 - wasm_bounds_checks - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_bounds_checks on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_bounds_checks=on"/
--- no_error_log
[error]
[crit]



=== TEST 70: flag directive - v8 - wasm_bounds_checks - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_bounds_checks off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_bounds_checks=off"/
--- no_error_log
[error]
[crit]



=== TEST 71: flag directive - v8 - wasm_stack_checks - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_stack_checks on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_stack_checks=on"/
--- no_error_log
[error]
[crit]



=== TEST 72: flag directive - v8 - wasm_stack_checks - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_stack_checks off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_stack_checks=off"/
--- no_error_log
[error]
[crit]



=== TEST 73: flag directive - v8 - wasm_enforce_bounds_checks - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_enforce_bounds_checks on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_enforce_bounds_checks=on"/
--- no_error_log
[error]
[crit]



=== TEST 74: flag directive - v8 - wasm_enforce_bounds_checks - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_enforce_bounds_checks off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_enforce_bounds_checks=off"/
--- no_error_log
[error]
[crit]



=== TEST 75: flag directive - v8 - wasm_math_intrinsics - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_math_intrinsics on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_math_intrinsics=on"/
--- no_error_log
[error]
[crit]



=== TEST 76: flag directive - v8 - wasm_math_intrinsics - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_math_intrinsics off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_math_intrinsics=off"/
--- no_error_log
[error]
[crit]



=== TEST 77: flag directive - v8 - wasm_inlining - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_inlining on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_inlining=on"/
--- no_error_log
[error]
[crit]



=== TEST 78: flag directive - v8 - wasm_inlining - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_inlining off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_inlining=off"/
--- no_error_log
[error]
[crit]



=== TEST 79: flag directive - v8 - wasm_inlining_budget_factor - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_inlining_budget_factor on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_inlining_budget_factor=on"/
--- no_error_log
[error]
[crit]



=== TEST 80: flag directive - v8 - wasm_inlining_budget_factor - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_inlining_budget_factor off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_inlining_budget_factor=off"/
--- no_error_log
[error]
[crit]



=== TEST 81: flag directive - v8 - wasm_inlining_max_size - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_inlining_max_size on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_inlining_max_size=on"/
--- no_error_log
[error]
[crit]



=== TEST 82: flag directive - v8 - wasm_inlining_max_size - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_inlining_max_size off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_inlining_max_size=off"/
--- no_error_log
[error]
[crit]



=== TEST 83: flag directive - v8 - wasm_speculative_inlining - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_speculative_inlining on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_speculative_inlining=on"/
--- no_error_log
[error]
[crit]



=== TEST 84: flag directive - v8 - wasm_speculative_inlining - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_speculative_inlining off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_speculative_inlining=off"/
--- no_error_log
[error]
[crit]



=== TEST 85: flag directive - v8 - trace_wasm_inlining - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag trace_wasm_inlining on;
        }
    }
--- error_log eval
qr/setting flag: "trace_wasm_inlining=on"/
--- no_error_log
[error]
[crit]



=== TEST 86: flag directive - v8 - trace_wasm_inlining - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag trace_wasm_inlining off;
        }
    }
--- error_log eval
qr/setting flag: "trace_wasm_inlining=off"/
--- no_error_log
[error]
[crit]



=== TEST 87: flag directive - v8 - trace_wasm_speculative_inlining - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag trace_wasm_speculative_inlining on;
        }
    }
--- error_log eval
qr/setting flag: "trace_wasm_speculative_inlining=on"/
--- no_error_log
[error]
[crit]



=== TEST 88: flag directive - v8 - trace_wasm_speculative_inlining - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag trace_wasm_speculative_inlining off;
        }
    }
--- error_log eval
qr/setting flag: "trace_wasm_speculative_inlining=off"/
--- no_error_log
[error]
[crit]



=== TEST 89: flag directive - v8 - wasm_type_canonicalization - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_type_canonicalization on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_type_canonicalization=on"/
--- no_error_log
[error]
[crit]



=== TEST 90: flag directive - v8 - wasm_type_canonicalization - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_type_canonicalization off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_type_canonicalization=off"/
--- no_error_log
[error]
[crit]



=== TEST 91: flag directive - v8 - wasm_loop_unrolling - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_loop_unrolling on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_loop_unrolling=on"/
--- no_error_log
[error]
[crit]



=== TEST 92: flag directive - v8 - wasm_loop_unrolling - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_loop_unrolling off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_loop_unrolling=off"/
--- no_error_log
[error]
[crit]



=== TEST 93: flag directive - v8 - wasm_loop_peeling - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_loop_peeling on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_loop_peeling=on"/
--- no_error_log
[error]
[crit]



=== TEST 94: flag directive - v8 - wasm_loop_peeling - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_loop_peeling off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_loop_peeling=off"/
--- no_error_log
[error]
[crit]



=== TEST 95: flag directive - v8 - wasm_fuzzer_gen_test - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_fuzzer_gen_test on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_fuzzer_gen_test=on"/
--- no_error_log
[error]
[crit]



=== TEST 96: flag directive - v8 - wasm_fuzzer_gen_test - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_fuzzer_gen_test off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_fuzzer_gen_test=off"/
--- no_error_log
[error]
[crit]



=== TEST 97: flag directive - v8 - print_wasm_code - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag print_wasm_code on;
        }
    }
--- error_log eval
qr/setting flag: "print_wasm_code=on"/
--- no_error_log
[error]
[crit]



=== TEST 98: flag directive - v8 - print_wasm_code - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag print_wasm_code off;
        }
    }
--- error_log eval
qr/setting flag: "print_wasm_code=off"/
--- no_error_log
[error]
[crit]



=== TEST 99: flag directive - v8 - print_wasm_code_function_index - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag print_wasm_code_function_index on;
        }
    }
--- error_log eval
qr/setting flag: "print_wasm_code_function_index=on"/
--- no_error_log
[error]
[crit]



=== TEST 100: flag directive - v8 - print_wasm_code_function_index - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag print_wasm_code_function_index off;
        }
    }
--- error_log eval
qr/setting flag: "print_wasm_code_function_index=off"/
--- no_error_log
[error]
[crit]



=== TEST 101: flag directive - v8 - print_wasm_stub_code - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag print_wasm_stub_code on;
        }
    }
--- error_log eval
qr/setting flag: "print_wasm_stub_code=on"/
--- no_error_log
[error]
[crit]



=== TEST 102: flag directive - v8 - print_wasm_stub_code - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag print_wasm_stub_code off;
        }
    }
--- error_log eval
qr/setting flag: "print_wasm_stub_code=off"/
--- no_error_log
[error]
[crit]



=== TEST 103: flag directive - v8 - asm_wasm_lazy_compilation - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag asm_wasm_lazy_compilation on;
        }
    }
--- error_log eval
qr/setting flag: "asm_wasm_lazy_compilation=on"/
--- no_error_log
[error]
[crit]



=== TEST 104: flag directive - v8 - asm_wasm_lazy_compilation - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag asm_wasm_lazy_compilation off;
        }
    }
--- error_log eval
qr/setting flag: "asm_wasm_lazy_compilation=off"/
--- no_error_log
[error]
[crit]



=== TEST 105: flag directive - v8 - wasm_lazy_compilation - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_lazy_compilation on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_lazy_compilation=on"/
--- no_error_log
[error]
[crit]



=== TEST 106: flag directive - v8 - wasm_lazy_compilation - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_lazy_compilation off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_lazy_compilation=off"/
--- no_error_log
[error]
[crit]



=== TEST 107: flag directive - v8 - wasm_lazy_validation - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_lazy_validation on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_lazy_validation=on"/
--- no_error_log
[error]
[crit]



=== TEST 108: flag directive - v8 - wasm_lazy_validation - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_lazy_validation off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_lazy_validation=off"/
--- no_error_log
[error]
[crit]



=== TEST 109: flag directive - v8 - wasm_simd_ssse3_codegen - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_simd_ssse3_codegen on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_simd_ssse3_codegen=on"/
--- no_error_log
[error]
[crit]



=== TEST 110: flag directive - v8 - wasm_simd_ssse3_codegen - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_simd_ssse3_codegen off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_simd_ssse3_codegen=off"/
--- no_error_log
[error]
[crit]



=== TEST 111: flag directive - v8 - wasm_code_gc - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_code_gc on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_code_gc=on"/
--- no_error_log
[error]
[crit]



=== TEST 112: flag directive - v8 - wasm_code_gc - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_code_gc off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_code_gc=off"/
--- no_error_log
[error]
[crit]



=== TEST 113: flag directive - v8 - trace_wasm_code_gc - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag trace_wasm_code_gc on;
        }
    }
--- error_log eval
qr/setting flag: "trace_wasm_code_gc=on"/
--- no_error_log
[error]
[crit]



=== TEST 114: flag directive - v8 - trace_wasm_code_gc - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag trace_wasm_code_gc off;
        }
    }
--- error_log eval
qr/setting flag: "trace_wasm_code_gc=off"/
--- no_error_log
[error]
[crit]



=== TEST 115: flag directive - v8 - stress_wasm_code_gc - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag stress_wasm_code_gc on;
        }
    }
--- error_log eval
qr/setting flag: "stress_wasm_code_gc=on"/
--- no_error_log
[error]
[crit]



=== TEST 116: flag directive - v8 - stress_wasm_code_gc - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag stress_wasm_code_gc off;
        }
    }
--- error_log eval
qr/setting flag: "stress_wasm_code_gc=off"/
--- no_error_log
[error]
[crit]



=== TEST 117: flag directive - v8 - wasm_max_initial_code_space_reservation - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_max_initial_code_space_reservation on;
        }
    }
--- error_log eval
qr/setting flag: "wasm_max_initial_code_space_reservation=on"/
--- no_error_log
[error]
[crit]



=== TEST 118: flag directive - v8 - wasm_max_initial_code_space_reservation - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_max_initial_code_space_reservation off;
        }
    }
--- error_log eval
qr/setting flag: "wasm_max_initial_code_space_reservation=off"/
--- no_error_log
[error]
[crit]



=== TEST 119: flag directive - v8 - experimental_wasm_allow_huge_modules - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_allow_huge_modules on;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_allow_huge_modules=on"/
--- no_error_log
[error]
[crit]



=== TEST 120: flag directive - v8 - experimental_wasm_allow_huge_modules - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag experimental_wasm_allow_huge_modules off;
        }
    }
--- error_log eval
qr/setting flag: "experimental_wasm_allow_huge_modules=off"/
--- no_error_log
[error]
[crit]



=== TEST 121: flag directive - v8 - trace_wasm - on
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag trace_wasm on;
        }
    }
--- error_log eval
qr/setting flag: "trace_wasm=on"/
--- no_error_log
[error]
[crit]



=== TEST 122: flag directive - v8 - trace_wasm - off
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag trace_wasm off;
        }
    }
--- error_log eval
qr/setting flag: "trace_wasm=off"/
--- no_error_log
[error]
[crit]



=== TEST 123: flag directive - v8 - perf_prof_annotate_wasm - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag perf_prof_annotate_wasm on;
        }
    }
--- error_log eval
qr/setting flag: "perf_prof_annotate_wasm=on"/
--- no_error_log
[error]
[crit]



=== TEST 124: flag directive - v8 - perf_prof_annotate_wasm - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag perf_prof_annotate_wasm off;
        }
    }
--- error_log eval
qr/setting flag: "perf_prof_annotate_wasm=off"/
--- no_error_log
[error]
[crit]



=== TEST 125: flag directive - v8 - vtune_prof_annotate_wasm - on
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag vtune_prof_annotate_wasm on;
        }
    }
--- error_log eval
qr/setting flag: "vtune_prof_annotate_wasm=on"/
--- no_error_log
[error]
[crit]



=== TEST 126: flag directive - v8 - vtune_prof_annotate_wasm - off
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag vtune_prof_annotate_wasm off;
        }
    }
--- error_log eval
qr/setting flag: "vtune_prof_annotate_wasm=off"/
--- no_error_log
[error]
[crit]



=== TEST 127: flag directive - v8 - wasm_max_mem_pages
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_max_mem_pages 65536;
        }
    }
--- error_log eval
qr/setting flag: "wasm_max_mem_pages=65536"/
--- no_error_log
[error]
[crit]



=== TEST 128: flag directive - v8 - wasm_max_table_size
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_max_table_size 10000000;
        }
    }
--- error_log eval
qr/setting flag: "wasm_max_table_size=10000000"/
--- no_error_log
[error]
[crit]



=== TEST 129: flag directive - v8 - wasm_max_code_space
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_max_code_space 4095;
        }
    }
--- error_log eval
qr/setting flag: "wasm_max_code_space=4095"/
--- no_error_log
[error]
[crit]



=== TEST 130: flag directive - v8 - wasm_tiering_budget
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_tiering_budget 1800000;
        }
    }
--- error_log eval
qr/setting flag: "wasm_tiering_budget=1800000"/
--- no_error_log
[error]
[crit]



=== TEST 131: flag directive - v8 - wasm_caching_threshold
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_caching_threshold 1000000;
        }
    }
--- error_log eval
qr/setting flag: "wasm_caching_threshold=1000000"/
--- no_error_log
[error]
[crit]



=== TEST 132: flag directive - v8 - wasm_tier_up_filter
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_tier_up_filter -1;
        }
    }
--- error_log eval
qr/setting flag: "wasm_tier_up_filter=-1"/
--- no_error_log
[error]
[crit]



=== TEST 133: flag directive - v8 - wasm_tier_mask_for_testing
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_tier_mask_for_testing 0;
        }
    }
--- error_log eval
qr/setting flag: "wasm_tier_mask_for_testing=0"/
--- no_error_log
[error]
[crit]



=== TEST 134: flag directive - v8 - wasm_debug_mask_for_testing
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag wasm_debug_mask_for_testing 0;
        }
    }
--- error_log eval
qr/setting flag: "wasm_debug_mask_for_testing=0"/
--- no_error_log
[error]
[crit]



=== TEST 135: flag directive - v8 - dump_wasm_module_path
--- SKIP: makes Nginx core dump
--- skip_eval: 4: $::nginxV !~ m/v8/
--- main_config
    wasm {
        v8 {
            flag dump_wasm_module_path "/tmp";
        }
    }
--- error_log eval
qr/setting flag: "dump_wasm_module_path=\/tmp"/
--- no_error_log
[error]
[crit]
