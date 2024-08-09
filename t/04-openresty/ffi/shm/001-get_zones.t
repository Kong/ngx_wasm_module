# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(7);
run_tests();

__DATA__

=== TEST 1: shm - get_zones()
get_zones() is silently called when resty.wasmx.shm module is loaded.

--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv1 1m
--- shm_queue: q1 1m
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            assert(shm.kv1 ~= nil)
            assert(shm.q1 ~= nil)
            assert(shm.metrics ~= nil)
        }

        echo ok;
    }
--- response_body
ok
--- no_error_log
[error]
[crit]
[emerg]
[alert]
[stub]



=== TEST 2: shm - get_zones(), no zones
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        access_by_lua_block {
            local shm = require "resty.wasmx.shm"

            assert(shm.metrics == nil)
        }

        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? no shm zones found/,
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 3: shm - get_zones(), bad pointer
A call with a bad pointer is simply ignored.

--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        access_by_lua_block {
            local ffi = require "ffi"
            local C = ffi.C

            ffi.cdef [[
                typedef struct ngx_wa_shm_t  ngx_wa_shm_t;

                typedef void (*ngx_wa_ffi_shm_get_zones_handler_pt)(ngx_wa_shm_t *shm);

                int ngx_wa_ffi_shm_get_zones(ngx_wa_ffi_shm_get_zones_handler_pt handler);
            ]]

            C.ngx_wa_ffi_shm_get_zones(nil)
        }

        echo ok;
    }
--- response_body
ok
--- no_error_log
[error]
[crit]
[emerg]
[alert]
[stub]
