# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

our $nginxV = $t::TestWasm::nginxV;

workers(2);
master_on();
plan tests => repeat_each() * (blocks() * 5);
run_tests();

__DATA__

=== TEST 1: kv
--- main_config
    wasm {
        shm_kv my_kv_1 1048576;
        shm_kv my_kv_2 65536;
    }
--- error_log eval
qr/\[info\] .*? \[wasm\] shm init/
--- no_error_log
[emerg]
[error]
[crit]
