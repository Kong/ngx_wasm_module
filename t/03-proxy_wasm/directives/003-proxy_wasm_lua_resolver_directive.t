# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: proxy_wasm_lua_resolver directive - missing Lua support in http{}
--- skip_eval: 6: $::nginxV =~ m/openresty/
--- config
    location /t {
        proxy_wasm_lua_resolver on;
    }
--- error_log eval
qr/\[emerg\] .*? \[wasm\] proxy_wasm_lua_resolver requires lua support/
--- no_error_log
[warn]
[error]
[alert]
[crit]
--- must_die



=== TEST 2: proxy_wasm_lua_resolver directive - missing Lua support in wasm{}
--- skip_eval: 6: $::nginxV =~ m/openresty/
--- main_config
    wasm {
        proxy_wasm_lua_resolver on;
    }
--- error_log eval
qr/\[emerg\] .*? \[wasm\] proxy_wasm_lua_resolver requires lua support/
--- no_error_log
[warn]
[error]
[alert]
[crit]
--- must_die
