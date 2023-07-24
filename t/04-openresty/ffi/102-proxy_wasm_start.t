# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

skip_no_openresty();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: start() - fail with no plan
--- wasm_modules: on_phases
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local ok, err = proxy_wasm.start()
            if not ok then
                return ngx.say(err)
            end

            ngx.say("failed")
        }
    }
--- response_body
plan not loaded and attached
--- no_error_log
[error]
[crit]



=== TEST 2: start() - fail with plan not loaded
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            local filters = {
                { name = "on_phases" },
            }

            local c_plan, err = proxy_wasm.new(filters)
            if not c_plan then
                return ngx.say(err)
            end

            local ok, err = proxy_wasm.start()
            if not ok then
                return ngx.say(err)
            end
        }

        echo failed;
    }
--- response_body
plan not loaded and attached
--- no_error_log
[error]
[crit]



=== TEST 3: start() - fail with plan loaded but not attached
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            local filters = {
                { name = "on_phases" },
            }

            local c_plan, err = proxy_wasm.new(filters)
            if not c_plan then
                return ngx.say(err)
            end

            local ok, err = proxy_wasm.load(c_plan)
            if not ok then
                return ngx.say(err)
            end

            ok, err = proxy_wasm.start()
            if not ok then
                return ngx.say(err)
            end
        }

        echo failed;
    }
--- response_body
plan not loaded and attached
--- no_error_log
[error]
[crit]



=== TEST 4: start() with plan loaded and attached on rewrite
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            local filters = {
                { name = "on_phases" },
            }

            local c_plan, err = proxy_wasm.new(filters)
            if not c_plan then
                return ngx.say(err)
            end

            local ok, err = proxy_wasm.load(c_plan)
            if not ok then
                return ngx.say(err)
            end

            ok, err = proxy_wasm.attach(c_plan)
            if not ok then
                return ngx.say(err)
            end

            ok, err = proxy_wasm.start()
            if not ok then
                return ngx.say(err)
            end
        }

        echo ok;
    }
--- request
POST /t
Hello world
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(configure|vm_start|request|response|log).*/
--- grep_error_log_out eval
qr/^#0 on_vm_start[^#]*
#0 on_configure, config_size: 0[^#]*
#\d+ on_request_headers, 3 headers[^#]*
#\d+ on_request_body, 11 bytes[^#]*
#\d+ on_response_headers, 5 headers[^#]*
#\d+ on_response_body, 3 bytes, eof: false[^#]*
#\d+ on_response_body, 0 bytes, eof: true[^#]*
#\d+ on_log[^#]*/
--- no_error_log
[error]



=== TEST 5: start() with plan loaded and attached on access
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            local filters = {
                { name = "on_phases" },
            }

            local c_plan, err = proxy_wasm.new(filters)
            if not c_plan then
                return ngx.say(err)
            end

            local ok, err = proxy_wasm.load(c_plan)
            if not ok then
                return ngx.say(err)
            end

            ok, err = proxy_wasm.attach(c_plan)
            if not ok then
                return ngx.say(err)
            end

            ok, err = proxy_wasm.start()
            if not ok then
                return ngx.say(err)
            end
        }

        echo ok;
    }
--- request
POST /t
Hello world
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(configure|vm_start|request|response|log).*/
--- grep_error_log_out eval
qr/^#0 on_vm_start[^#]*
#0 on_configure, config_size: 0[^#]*
#\d+ on_request_headers, 3 headers[^#]*
#\d+ on_request_body, 11 bytes[^#]*
#\d+ on_response_headers, 5 headers[^#]*
#\d+ on_response_body, 3 bytes, eof: false[^#]*
#\d+ on_response_body, 0 bytes, eof: true[^#]*
#\d+ on_log[^#]*/
--- no_error_log
[error]



=== TEST 6: start() - fail if attempt to start on init_worker
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"

        assert(proxy_wasm.start())
    }
--- config
    location /t {
        echo ok;
    }
--- request
POST /t
Hello world
--- ignore_response_body
--- error_log eval
qr/start must be called from 'rewrite' or 'access' phase/
--- no_error_log
[crit]
[emerg]
