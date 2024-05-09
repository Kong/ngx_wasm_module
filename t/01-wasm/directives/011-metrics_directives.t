# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: slab_size directive - sanity
--- main_config
    wasm {
        metrics {
            slab_size 12k;
        }
    }
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 2: slab_size directive - too small
--- main_config
    wasm {
        metrics {
            slab_size 1k;
        }
    }
--- error_log eval
qr/\[emerg\] .*? \[wasm\] shm size of \d+ bytes is too small, minimum required is 12288 bytes/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 3: slab_size directive - invalid size
--- main_config
    wasm {
        metrics {
            slab_size 1x;
        }
    }
--- error_log eval
qr/\[emerg\] .*? \[wasm\] invalid shm size "1x"/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 4: slab_size directive - duplicate
--- main_config
    wasm {
        metrics {
            slab_size 12k;
            slab_size 12k;
        }
    }
--- error_log: is duplicate
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 5: max_metric_name_length directive - sanity
--- main_config
    wasm {
        metrics {
            max_metric_name_length 64;
        }
    }
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 6: max_metric_name_length directive - invalid value
--- main_config
    wasm {
        metrics {
            max_metric_name_length -1;
        }
    }
--- error_log: invalid value
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 7: max_metric_name_length directive - invalid value (5)
--- main_config
    wasm {
        metrics {
            max_metric_name_length 5;
        }
    }
--- error_log: value too small (min: 6)
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 8: max_metric_name_length directive - duplicate
--- main_config
    wasm {
        metrics {
            max_metric_name_length 64;
            max_metric_name_length 64;
        }
    }
--- error_log: is duplicate
--- no_error_log
[error]
[crit]
--- must_die
