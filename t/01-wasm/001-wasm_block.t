# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: wasm{} - empty block
--- main_config
    wasm {}
--- error_log
initializing "main" wasm VM
--- no_error_log
[error]
[emerg]



=== TEST 2: wasm{} - no block, no VM
--- main_config
--- no_error_log
wasm VM
[error]
[emerg]



=== TEST 3: wasm{} - duplicated block
--- main_config
    wasm {}
    wasm {}
--- error_log
"wasm" directive is duplicate
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 4: wasm{} - unknown directive
--- main_config
    wasm {
        unknown;
    }
--- error_log
unknown directive "unknown"
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 5: wasm{} - invalid content
--- main_config
    wasm {
        {
    }
--- error_log
unexpected "{"
--- no_error_log
[error]
[crit]
--- must_die
