# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: resolver directive - sanity
--- valgrind
--- main_config
    wasm {
        resolver 8.8.8.8;
    }
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 2: resolver directive - duplicated
--- main_config
    wasm {
        resolver 8.8.8.8;
        resolver 8.8.8.8;
    }
--- error_log
"resolver" directive is duplicate
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 3: resolver directive - bad IP
--- main_config
    wasm {
        resolver bad;
    }
--- error_log
host not found in resolver "bad"
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 4: resolver_timeout directive - sanity
--- main_config
    wasm {
        resolver         8.8.8.8;
        resolver_timeout 5s;
    }
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 5: resolver_timeout directive - bad value
--- main_config
    wasm {
        resolver         8.8.8.8;
        resolver_timeout bad;
    }
--- error_log
"resolver_timeout" directive invalid value
--- no_error_log
[error]
[crit]
--- must_die
