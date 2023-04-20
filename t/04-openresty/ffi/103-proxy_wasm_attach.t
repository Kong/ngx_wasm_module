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
            local proxy_wasm = require "resty.http.proxy_wasm"

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
            local proxy_wasm = require "resty.http.proxy_wasm"
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



=== TEST 3: attach() - load() + attach() in rewrite
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
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
--- grep_error_log eval: qr/#\d+ on_(configure|vm_start|request|response|log).*/
--- grep_error_log_out eval
qr/^[^#]*#0 on_configure, config_size: 0[^#]*
#0 on_vm_start[^#]*
#\d+ on_request_headers, 3 headers.*
#\d+ on_request_body, 11 bytes.*
#\d+ on_response_headers, 5 headers.*
#\d+ on_response_body, 3 bytes, eof: false.*
#\d+ on_response_body, 0 bytes, eof: true.*
#\d+ on_log.*/
--- no_error_log
[error]



=== TEST 4: attach() - load() in init_worker + attach() in rewrite
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
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
            local proxy_wasm = require "resty.http.proxy_wasm"
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
--- grep_error_log eval: qr/#\d+ on_(configure|request|response|log).*/
--- grep_error_log_out eval
qr/^[^#]*#0 on_configure, config_size: 0[^#]*
#\d+ on_request_headers, 3 headers.*
#\d+ on_request_body, 11 bytes.*
#\d+ on_response_headers, 5 headers.*
#\d+ on_response_body, 3 bytes, eof: false.*
#\d+ on_response_body, 0 bytes, eof: true.*
#\d+ on_log.*/
--- no_error_log
[error]



=== TEST 5: attach() - load() in init_worker + attach() in access
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
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
            local proxy_wasm = require "resty.http.proxy_wasm"
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
--- grep_error_log eval: qr/#\d+ on_(configure|request|response|log).*/
--- grep_error_log_out eval
qr/^[^#]*#0 on_configure, config_size: 0[^#]*
#\d+ on_request_headers, 3 headers.*
#\d+ on_request_body, 11 bytes.*
#\d+ on_response_headers, 5 headers.*
#\d+ on_response_body, 3 bytes, eof: false.*
#\d+ on_response_body, 0 bytes, eof: true.*
#\d+ on_log.*/
--- no_error_log
[error]



=== TEST 6: attach() - sanity on trap (on_request_headers)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
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
            local proxy_wasm = require "resty.http.proxy_wasm"
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
    qr/\[crit\] .*? panicked at 'trap'/,
    qr/\[error\] .*? (error while executing at wasm backtrace:|(Uncaught RuntimeError)?unreachable)/
]



=== TEST 7: attach() - plan is garbage collected after execution
--- skip_no_debug: 4
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
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



=== TEST 8: attach() - plan cannot be attached more than once in a given request
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        rewrite_by_lua_block {
            local proxy_wasm = require "resty.http.proxy_wasm"
            local filters = {
                { name = "on_phases" },
            }

            local c_plan = assert(proxy_wasm.new(filters))
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
--- request
POST /t
Hello world
--- response_body
ok
--- grep_error_log eval: qr/#\d+ on_(configure|request|response|log).*/
--- grep_error_log_out eval
qr/^[^#]*#0 on_configure, config_size: 0[^#]*
#\d+ on_request_headers, 3 headers.*
#\d+ on_request_body, 11 bytes.*
#\d+ on_response_headers, 5 headers.*
#\d+ on_response_body, 3 bytes, eof: false.*
#\d+ on_response_body, 0 bytes, eof: true.*
#\d+ on_log.*/
--- no_error_log
[error]



=== TEST 9: attach() - fail if attempt to attach on init_worker
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
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
            local proxy_wasm = require "resty.http.proxy_wasm"
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
            local proxy_wasm = require "resty.http.proxy_wasm"
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
        local proxy_wasm = require "resty.http.proxy_wasm"
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
            local proxy_wasm = require "resty.http.proxy_wasm"
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
--- grep_error_log eval: qr/\*\d+.*?(resuming|new instance|reusing instance|finalizing|freeing|now|trap in|filter chain).*/
--- grep_error_log_out eval
[
qr/^\*\d+ proxy_wasm initializing filter chain \(nfilters: 1, isolation: 1\).*
.*?\*\d+ proxy_wasm "hostcalls" filter reusing instance.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) finalizing context.*
\*\d+ proxy_wasm freeing stream context #\d+ \(main: 1\).*/,
qr/^\*\d+ proxy_wasm initializing filter chain \(nfilters: 1, isolation: 1\).*
.*?\*\d+ proxy_wasm "hostcalls" filter reusing instance.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) finalizing context.*
\*\d+ proxy_wasm freeing stream context #\d+ \(main: 1\).*/
]



=== TEST 13: attach() - opts.isolation: fallback to subsystem context setting
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
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
            local proxy_wasm = require "resty.http.proxy_wasm"
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
--- grep_error_log eval: qr/(\*\d+.*?(resuming|new instance|reusing instance|finalizing|freeing|now|trap in|filter chain)|#\d+ on_(configure|vm_start)).*/
--- grep_error_log_out eval
[
qr/^[^#]*#0 on_configure[^#]*
#0 on_vm_start[^#*]*
\*\d+ proxy_wasm initializing filter chain \(nfilters: 1, isolation: 2\).*
\*\d+ proxy_wasm "hostcalls" filter new instance.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "rewrite" phase[^#*]*
#0 on_configure[^#*]*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) finalizing context.*
\*\d+ proxy_wasm freeing stream context #\d+ \(main: 1\).*
\*\d+ wasm freeing "hostcalls" instance/,
qr/^\*\d+ proxy_wasm initializing filter chain \(nfilters: 1, isolation: 2\).*
\*\d+ proxy_wasm "hostcalls" filter new instance.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "rewrite" phase[^#*]*
#0 on_configure[^#*]*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) finalizing context.*
\*\d+ proxy_wasm freeing stream context #\d+ \(main: 1\).*
\*\d+ wasm freeing "hostcalls" instance/
]



=== TEST 14: attach() - opts.isolation: override directive
--- wasm_modules: hostcalls
--- http_config
    init_worker_by_lua_block {
        local proxy_wasm = require "resty.http.proxy_wasm"
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
            local proxy_wasm = require "resty.http.proxy_wasm"
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
--- grep_error_log eval: qr/(\*\d+.*?(resuming|new instance|reusing instance|finalizing|freeing|now|trap in|filter chain)|#\d+ (on_configure|on_vm_start)).*/
--- grep_error_log_out eval
[
qr/^[^#]*#0 on_configure[^#]*
#0 on_vm_start[^#*]*
\*\d+ proxy_wasm initializing filter chain \(nfilters: 1, isolation: 1\).*
.*?\*\d+ proxy_wasm "hostcalls" filter reusing instance.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) finalizing context.*
\*\d+ proxy_wasm freeing stream context #\d+ \(main: 1\)/,
qr/^\*\d+ proxy_wasm initializing filter chain \(nfilters: 1, isolation: 1\).*
.*?\*\d+ proxy_wasm "hostcalls" filter reusing instance.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "rewrite" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "header_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "body_filter" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "log" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) resuming in "done" phase.*
\*\d+ proxy_wasm "hostcalls" filter \(1\/1\) finalizing context.*
\*\d+ proxy_wasm freeing stream context #\d+ \(main: 1\)/
]
