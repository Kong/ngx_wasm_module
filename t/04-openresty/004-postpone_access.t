# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();
skip_no_debug();

plan_tests(3);
run_tests();

__DATA__

=== TEST 1: wasm_postpone_access - default (Lua on), auto-enabled
Note: This suite assumes the Lua module be specified first in compilation order.
--- config
    location /t {
        access_by_lua_block {

        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/(wasm|lua) access handler/
--- grep_error_log_out
lua access handler
wasm access handler
lua access handler
wasm access handler



=== TEST 2: wasm_postpone_access - default (Lua off)
--- http_config
    access_by_lua_no_postpone on;
--- config
    location /t {
        access_by_lua_block {

        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/(wasm|lua) access handler/
--- grep_error_log_out
lua access handler
wasm access handler



=== TEST 3: wasm_postpone_access - disabled (Lua on)
--- config
    location /t {
        wasm_postpone_access off;

        access_by_lua_block {

        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/(wasm|lua) access handler/
--- grep_error_log_out
lua access handler
wasm access handler
lua access handler



=== TEST 4: wasm_postpone_access - disabled (Lua off)
--- http_config
    access_by_lua_no_postpone on;
--- config
    location /t {
        wasm_postpone_access off;

        access_by_lua_block {

        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/(wasm|lua) access handler/
--- grep_error_log_out
lua access handler
wasm access handler



=== TEST 5: wasm_postpone_access - enabled (Lua on)
--- config
    location /t {
        wasm_postpone_access on;

        access_by_lua_block {

        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/(wasm|lua) access handler/
--- grep_error_log_out
lua access handler
wasm access handler
lua access handler
wasm access handler



=== TEST 6: wasm_postpone_access - enabled (Lua off)
--- http_config
    access_by_lua_no_postpone on;
--- config
    location /t {
        wasm_postpone_access on;

        access_by_lua_block {

        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/(wasm|lua) access handler/
--- grep_error_log_out
lua access handler
wasm access handler
wasm access handler
