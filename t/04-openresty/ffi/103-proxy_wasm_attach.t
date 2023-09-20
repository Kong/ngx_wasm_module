# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

skip_no_openresty();

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: attach() - bad plan (bad argument)
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"

            local pok, perr = pcall(proxy_wasm.attach, {})
            if not pok then
                return ngx.say(perr)
            end

            ngx.say("failed")
        }
    }
--- response_body
plan must be a cdata object
--- no_error_log
[error]
[crit]



=== TEST 2: attach() - bad plan (not loaded)
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

            local ok, err = proxy_wasm.attach(c_plan)
            if not ok then
                return ngx.say(err)
            end
        }

        echo failed;
    }
--- response_body
plan not loaded
--- no_error_log
[error]
[crit]



=== TEST 3: attach() - plan already attached
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

            assert(proxy_wasm.load(c_plan))
            assert(proxy_wasm.attach(c_plan))

            local ok, err = proxy_wasm.attach(c_plan)
            if not ok then
                ngx.log(ngx.ERR, err)
            end

            assert(proxy_wasm.start())
        }

        echo ok;
    }
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_.*/
--- grep_error_log_out eval
qr/^[^#]*#0 on_vm_start[^#]*
#0 on_configure, config_size: 0[^#]*
#\d+ on_request_headers, 2 headers[^#]*
#\d+ on_response_headers, 5 headers[^#]*
#\d+ on_response_body, 3 bytes, eof: false[^#]*
#\d+ on_response_body, 0 bytes, eof: true[^#]*
#\d+ on_done[^#]*
#\d+ on_log[^#]*$/
--- error_log eval
qr/\[error\] .*? previous plan already attached/



=== TEST 4: attach() - load() + attach() in rewrite
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

            assert(proxy_wasm.start())
        }

        echo ok;
    }
--- request
POST /t
Hello world
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_.*/
--- grep_error_log_out eval
qr/^[^#]*#0 on_vm_start[^#]*
#0 on_configure, config_size: 0[^#]*
#\d+ on_request_headers, 3 headers[^#]*
#\d+ on_request_body, 11 bytes[^#]*
#\d+ on_response_headers, 5 headers[^#]*
#\d+ on_response_body, 3 bytes, eof: false[^#]*
#\d+ on_response_body, 0 bytes, eof: true[^#]*
#\d+ on_done[^#]*
#\d+ on_log[^#]*$/
--- no_error_log
[error]



=== TEST 5: attach() - load() in init_worker + attach() in rewrite
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "on_phases" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        _G.c_plan = c_plan
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            local c_plan = _G.c_plan

            local ok, err = proxy_wasm.attach(c_plan)
            if not ok then
                return ngx.say(err)
            end

            assert(proxy_wasm.start())
        }

        echo ok;
    }
--- request
POST /t
Hello world
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_.*/
--- grep_error_log_out eval
qr/^[^#]*#0 on_vm_start[^#]*
#0 on_configure, config_size: 0[^#]*
#\d+ on_request_headers, 3 headers[^#]*
#\d+ on_request_body, 11 bytes[^#]*
#\d+ on_response_headers, 5 headers[^#]*
#\d+ on_response_body, 3 bytes, eof: false[^#]*
#\d+ on_response_body, 0 bytes, eof: true[^#]*
#\d+ on_done[^#]*
#\d+ on_log[^#]*$/
--- no_error_log
[error]



=== TEST 6: attach() - load() in init_worker + attach() in access
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "on_phases" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        _G.c_plan = c_plan
    }
--- config
    location /t {
        access_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            local c_plan = _G.c_plan

            local ok, err = proxy_wasm.attach(c_plan)
            if not ok then
                return ngx.say(err)
            end

            assert(proxy_wasm.start())
        }

        echo ok;
    }
--- request
POST /t
Hello world
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_.*/
--- grep_error_log_out eval
qr/^[^#]*#0 on_vm_start[^#]*
#0 on_configure, config_size: 0[^#]*
#\d+ on_request_headers, 3 headers[^#]*
#\d+ on_request_body, 11 bytes[^#]*
#\d+ on_response_headers, 5 headers[^#]*
#\d+ on_response_body, 3 bytes, eof: false[^#]*
#\d+ on_response_body, 0 bytes, eof: true[^#]*
#\d+ on_done[^#]*
#\d+ on_log[^#]*$/
--- no_error_log
[error]



=== TEST 7: attach() - sanity on trap (on_request_headers)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls", config = "test=/t/trap" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        _G.c_plan = c_plan
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            local c_plan = _G.c_plan

            local ok, err = proxy_wasm.attach(c_plan)
            if not ok then
                return ngx.say(err)
            end

            local ok, err = proxy_wasm.start()
            if not ok then
                return ngx.say(err)
            end
        }

        echo ok;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[crit\] .*? panicked at/,
    qr/custom trap/,
]



=== TEST 8: attach() - plan is garbage collected after execution
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            local filters = {
                { name = "on_phases" },
            }

            local c_plan = assert(proxy_wasm.new(filters))
            assert(proxy_wasm.load(c_plan))
            assert(proxy_wasm.attach(c_plan))
            assert(proxy_wasm.start())
        }

        echo ok;
    }
--- response_body
ok
--- shutdown_error_log eval
qr/\[debug\] .*? wasm freeing plan/
--- no_error_log
[error]



=== TEST 9: attach() - fail if attempt to attach on init_worker
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "on_phases" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))
        assert(proxy_wasm.attach(c_plan))
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
qr/attach must be called from 'rewrite' or 'access' phase/
--- no_error_log
[emerg]
[crit]



=== TEST 10: attach() - bad options (bad argument)
--- wasm_modules: on_phases
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            local filters = {
                { name = "on_phases" },
            }

            local c_plan = assert(proxy_wasm.new(filters))
            local pok, perr = pcall(proxy_wasm.attach, c_plan, false)
            if not pok then
                return ngx.say(perr)
            end

            ngx.say("failed")
        }
    }
--- response_body
opts must be a table
--- no_error_log
[error]
[crit]



=== TEST 11: attach() - bad opts.isolation (bad argument)
--- wasm_modules: on_phases
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            local filters = {
                { name = "on_phases" },
            }

            local c_plan = assert(proxy_wasm.new(filters))
            local pok, perr = pcall(proxy_wasm.attach, c_plan, {
                isolation = -1,
            })
            if not pok then
                return ngx.say(perr)
            end

            ngx.say("failed")
        }
    }
--- response_body
bad opts.isolation value: -1
--- no_error_log
[error]
[crit]



=== TEST 12: attach() - opts.isolation: default (none)
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        _G.c_plan = c_plan
    }
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- request eval
["GET /t", "GET /t"]
--- error_code eval
[200, 200]
--- ignore_response_body
--- grep_error_log eval: qr/\*\d+.*?(resuming|new instance|reusing instance|finalizing|freeing|now|trap in|initializing filter chain).*/
--- grep_error_log_out eval
[
qr/\A\*\d+ proxy_wasm initializing filter chain \(nfilters: 1, isolation: 1\)[^#*]*
\*\d+ .*? "hostcalls" filter reusing instance[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 1\/1 finalizing context[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/1\)[^#*]*\Z/,
qr/\A\*\d+ proxy_wasm initializing filter chain \(nfilters: 1, isolation: 1\)[^#*]*
\*\d+ .*? "hostcalls" filter reusing instance[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 1\/1 finalizing context[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/1\)[^#*]*\Z/,
]



=== TEST 13: attach() - opts.isolation: fallback to subsystem context setting
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        _G.c_plan = c_plan
    }
--- config
    proxy_wasm_isolation stream;

    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan))
            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- request eval
["GET /t", "GET /t"]
--- error_code eval
[200, 200]
--- ignore_response_body
--- grep_error_log eval: qr/(\*\d+.*?(resuming|new instance|reusing instance|finalizing|freeing|now|trap in|initializing filter chain)|#\d+ on_(configure|vm_start)).*/
--- grep_error_log_out eval
[
qr/\A[^#]*#0 on_vm_start[^#*]*
#0 on_configure[^#*]*
\*\d+ proxy_wasm initializing filter chain \(nfilters: 1, isolation: 2\)[^#*]*
\*\d+ .*? "hostcalls" filter new instance[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_request_headers" step in "rewrite" phase[^#*]*
#0 on_configure[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 1\/1 finalizing context[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/1\)[^#*]*
\*\d+ wasm freeing "hostcalls" instance[^#*]*\Z/,
qr/\A\*\d+ proxy_wasm initializing filter chain \(nfilters: 1, isolation: 2\)[^#*]*
\*\d+ .*? "hostcalls" filter new instance[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_request_headers" step in "rewrite" phase[^#*]*
#0 on_configure[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 1\/1 finalizing context[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/1\)[^#*]*
\*\d+ wasm freeing "hostcalls" instance[^#*]*\Z/,
]



=== TEST 14: attach() - opts.isolation: override directive
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.wasmx.proxy_wasm"
        local filters = {
            { name = "hostcalls" },
        }

        local c_plan = assert(proxy_wasm.new(filters))
        assert(proxy_wasm.load(c_plan))

        _G.c_plan = c_plan
    }
--- config
    proxy_wasm_isolation stream;

    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.wasmx.proxy_wasm"
            assert(proxy_wasm.attach(_G.c_plan, {
                isolation = proxy_wasm.isolations.NONE
            }))
            assert(proxy_wasm.start())
            ngx.say("ok")
        }
    }
--- request eval
["GET /t", "GET /t"]
--- error_code eval
[200, 200]
--- ignore_response_body
--- grep_error_log eval: qr/(\*\d+.*?(resuming|new instance|reusing instance|finalizing|freeing|now|trap in|initializing filter chain)|#\d+ (on_configure|on_vm_start)).*/
--- grep_error_log_out eval
[
qr/\A[^#]*#0 on_vm_start[^#*]*
#0 on_configure[^#*]*
\*\d+ proxy_wasm initializing filter chain \(nfilters: 1, isolation: 1\)[^#*]*
\*\d+ .*? "hostcalls" filter reusing instance[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 1\/1 finalizing context[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/1\)[^#*]*\Z/,
qr/\A\*\d+ proxy_wasm initializing filter chain \(nfilters: 1, isolation: 1\)[^#*]*
\*\d+ .*? "hostcalls" filter reusing instance[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_request_headers" step in "rewrite" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_headers" step in "header_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_response_body" step in "body_filter" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_log" step in "log" phase[^#*]*
\*\d+ .*? filter 1\/1 resuming "on_done" step in "done" phase[^#*]*
\*\d+ .*? filter 1\/1 finalizing context[^#*]*
\*\d+ .*? filter freeing context #\d+ \(1\/1\)[^#*]*\Z/,
]
