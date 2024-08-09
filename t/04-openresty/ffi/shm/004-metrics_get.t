# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;
use t::TestWasmX::Lua;

skip_no_openresty();

plan_tests(9);
run_tests();

__DATA__

=== TEST 1: shm_metrics - get(), sanity
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on=request_headers \
                              n_increments=13 \
                              test=/t/metrics/increment_counters \
                              metrics=c1';

        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on=request_headers \
                              test=/t/metrics/toggle_gauges \
                              metrics=g1';

        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on=request_headers \
                              test=/t/metrics/record_histograms \
                              metrics=h1 \
                              value=10';

        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on=request_headers \
                              test=/t/metrics/record_histograms \
                              metrics=h1 \
                              value=100';

        access_by_lua_block {
            local pretty = require "pl.pretty"
            local shm = require "resty.wasmx.shm"

            local metrics = shm.metrics

            local keys = {
                "pw.hostcalls.c1",
                "pw.hostcalls.g1",
                "pw.hostcalls.h1"
            }

            for i, k in pairs(keys) do
                local m = metrics:get(k)
                ngx.log(ngx.INFO, k .. ": " .. pretty.write(m, ""))
            end
        }

        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? pw\.hostcalls\.c1: \{type="counter",value=13}/,
    qr/\[info\] .*? pw\.hostcalls\.g1: \{type="gauge",value=1}/,
    qr/\[info\] .*? pw\.hostcalls\.h1: \{type="histogram",value=\{\[4294967295]=0,\[128]=1,\[16]=1}}/,
]
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 2: shm_metrics - get(), invalid name
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        access_by_lua_block {
            local pretty = require "pl.pretty"
            local shm = require "resty.wasmx.shm"

            local metrics = shm.metrics

            local k = "invalid_name"
            local m = metrics:get(k)

            ngx.log(ngx.INFO, k .. ": " .. pretty.write(m, ""))
        }

        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? invalid_name: nil/,
]
--- no_error_log
[error]
[crit]
[emerg]
[alert]
[stub]
[stub]



=== TEST 3: shm_metrics - get(), non-initialized
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on_configure=define_metrics \
                              on=request_headers \
                              metrics=c1,g1,h1';

        access_by_lua_block {
            local pretty = require "pl.pretty"
            local shm = require "resty.wasmx.shm"

            local metrics = shm.metrics

            local keys = {
                "pw.hostcalls.c1",
                "pw.hostcalls.g1",
                "pw.hostcalls.h1"
            }

            for i, k in pairs(keys) do
                local m = metrics:get(k)
                ngx.log(ngx.INFO, k .. ": " .. pretty.write(m, ""))
            end
        }

        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/\[info\] .*? pw\.hostcalls\.c1: \{type="counter",value=0}/,
    qr/\[info\] .*? pw\.hostcalls\.g1: \{type="gauge",value=0}/,
    qr/\[info\] .*? pw\.hostcalls\.h1: \{type="histogram",value=\{\[4294967295]=0}}/,
]
--- no_error_log
[error]
[crit]
[emerg]
[alert]



=== TEST 4: shm_metrics - get(), bad pointer
A call with bad a pointer is simply ignored.

--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- shm_kv: kv1 1m
--- config
    location /t {
        access_by_lua_block {
            local ffi = require "ffi"
            local C = ffi.C

            ffi.cdef [[
                typedef unsigned char  u_char;

                int ngx_wa_ffi_shm_get_metric(ngx_str_t *k,
                                              u_char *mbuf, size_t mbs,
                                              u_char *hbuf, size_t hbs);
            ]]

            C.ngx_wa_ffi_shm_get_metric(nil, nil, 0, nil, 0)
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
[stub]
[stub]
