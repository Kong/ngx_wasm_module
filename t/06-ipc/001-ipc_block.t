# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_no_ipc();

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: ipc{} - empty block
--- skip_no_debug
--- valgrind
--- main_config
    ipc {}
--- error_log
ipc core module initializing
--- no_error_log
wasm core module initializing
[error]
[crit]
[emerg]



=== TEST 2: ipc{} - with wasm{} block
--- skip_no_debug
--- valgrind
--- main_config
    ipc {}
    wasm {}
--- error_log
ipc core module initializing
wasm core module initializing
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 3: ipc{} - duplicated block
--- main_config
    ipc {}
    ipc {}
--- error_log
"ipc" directive is duplicate
--- no_error_log
ipc core module initializing
[error]
[crit]
[alert]
--- must_die
