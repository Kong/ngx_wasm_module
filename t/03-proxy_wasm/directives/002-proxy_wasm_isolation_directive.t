# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 6);

run_tests();

__DATA__

=== TEST 1: proxy_wasm_isolation directive - no wasm{} configuration block
--- main_config
--- config
    proxy_wasm_isolation none;
--- error_log eval
qr/\[emerg\] .*? "proxy_wasm_isolation" directive is specified but config has no "wasm" section/
--- no_error_log
[warn]
[error]
[alert]
[crit]
--- must_die



=== TEST 2: proxy_wasm_isolation directive - invalid number of arguments
--- config
    location /t {
        proxy_wasm_isolation none none;
    }
--- error_log eval
qr/\[emerg\] .*? invalid number of arguments in "proxy_wasm_isolation" directive/
--- no_error_log
[warn]
[error]
[alert]
[crit]
--- must_die
