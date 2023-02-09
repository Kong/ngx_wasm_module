# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

skip_no_openresty();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: load() - sanity in init_worker
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
        local filters = {
            { name = "on_phases" },
        }

        local c_plan, err = proxy_wasm.new(filters)
        if not c_plan then
            ngx.log(ngx.ERR, err)
            return
        end

        local ok, err = proxy_wasm.load(c_plan)
        if not ok then
            ngx.log(ngx.ERR, err)
            return
        end
    }
--- grep_error_log eval: qr/#\d+ on_configure.*/
--- grep_error_log_out eval
qr/#0 on_configure, config_size: 0.*/
--- no_error_log
[error]
[crit]



=== TEST 2: load() - with a ticking filter
--- wasm_modules: on_tick
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
        local filters = {
            { name = "on_tick" },
        }

        local c_plan, err = proxy_wasm.new(filters)
        if not c_plan then
            ngx.log(ngx.ERR, err)
            return
        end

        local ok, err = proxy_wasm.load(c_plan)
        if not ok then
            ngx.log(ngx.ERR, err)
            return
        end
    }
--- error_log eval
[
    qr/\[info\] \d+#\d+: ticking/,
    qr/\[info\] \d+#\d+: ticking/,
]
--- no_error_log
[error]



=== TEST 3: load() - bad argument
--- http_config
    init_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
        proxy_wasm.load({})
    }
--- error_log eval
qr/\[error\] .*? plan should be a cdata object/
--- no_error_log
[crit]
[emerg]
--- must_die
