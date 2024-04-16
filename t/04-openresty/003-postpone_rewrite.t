# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_hup(); # HUP mode /ver requests trigger many rewrite handler calls
skip_no_openresty();
skip_no_debug();

plan_tests(3);
run_tests();

__DATA__

=== TEST 1: wasm_postpone_rewrite - default (Lua on), auto-enabled
Note: This suite assumes the Lua module be specified first in compilation order.
--- config
    location /t {
        rewrite_by_lua_block {

        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/(wasm|lua) rewrite handler/
--- grep_error_log_out
lua rewrite handler
wasm rewrite handler
lua rewrite handler
wasm rewrite handler



=== TEST 2: wasm_postpone_rewrite - default (Lua off)
--- http_config
    rewrite_by_lua_no_postpone on;
--- config
    location /t {
        rewrite_by_lua_block {

        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/(wasm|lua) rewrite handler/
--- grep_error_log_out
lua rewrite handler
wasm rewrite handler



=== TEST 3: wasm_postpone_rewrite - disabled (Lua on)
--- config
    location /t {
        wasm_postpone_rewrite off;

        rewrite_by_lua_block {

        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/(wasm|lua) rewrite handler/
--- grep_error_log_out
lua rewrite handler
wasm rewrite handler
lua rewrite handler



=== TEST 4: wasm_postpone_rewrite - disabled (Lua off)
--- http_config
    rewrite_by_lua_no_postpone on;
--- config
    location /t {
        wasm_postpone_rewrite off;

        rewrite_by_lua_block {

        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/(wasm|lua) rewrite handler/
--- grep_error_log_out
lua rewrite handler
wasm rewrite handler



=== TEST 5: wasm_postpone_rewrite - enabled (Lua on)
--- config
    location /t {
        wasm_postpone_rewrite on;

        rewrite_by_lua_block {

        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/(wasm|lua) rewrite handler/
--- grep_error_log_out
lua rewrite handler
wasm rewrite handler
lua rewrite handler
wasm rewrite handler



=== TEST 6: wasm_postpone_rewrite - enabled (Lua off)
--- http_config
    rewrite_by_lua_no_postpone on;
--- config
    location /t {
        wasm_postpone_rewrite on;

        rewrite_by_lua_block {

        }

        content_by_lua_block {
            ngx.say("ok")
        }
    }
--- response_body
ok
--- grep_error_log eval: qr/(wasm|lua) rewrite handler/
--- grep_error_log_out
lua rewrite handler
wasm rewrite handler
wasm rewrite handler
