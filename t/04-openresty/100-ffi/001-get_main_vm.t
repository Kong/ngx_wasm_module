# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

skip_no_openresty();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: get_main_vm() - no VM
--- config
    location /t {
        content_by_lua_block {
            local wasm = require "resty.wasm"
            local vm = wasm.get_main_vm()

            ngx.say("vm: ", tostring(vm))
        }
    }
--- response_body
vm: cdata<struct ngx_wavm_t *>: NULL
--- no_error_log
[error]
[crit]



=== TEST 2: get_main_vm() - main VM
--- main_config
    wasm {}
--- config
    location /t {
        content_by_lua_block {
            local wasm = require "resty.wasm"
            local vm = wasm.get_main_vm()

            ngx.say("vm: ", tostring(vm))
        }
    }
--- response_body eval
qr/vm: cdata<struct ngx_wavm_t \*>: 0x[0-9a-f]+/
--- no_error_log
[error]
[crit]
