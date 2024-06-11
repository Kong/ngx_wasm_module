# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

skip_no_openresty();

add_block_preprocessor(sub {
    my $block = shift;
    if (!defined $block->wasm_modules) {
        $block->set_value("wasm_modules", "ngx_lua_tests");
    }
});

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: Lua bridge - Lua chunk arguments and return values
--- valgrind
--- config
    location /t {
        wasm_call log ngx_lua_tests test_lua_argsrets;
        return 200;
    }
--- error_log eval
qr/\[info\] .*? arg: argument/
--- no_error_log
[error]
[crit]



=== TEST 2: Lua bridge - bad Lua chunk
--- valgrind
--- config
    location /t {
        wasm_call log ngx_lua_tests test_bad_lua_chunk;
        return 200;
    }
--- error_log eval
[
    qr/\[error\] .*? failed to load inlined Lua code: \[string "bad_lua_chunk"\]:1: unexpected symbol near '<eof>'/,
    qr/host trap \(internal error\)/,
]
--- no_error_log
[crit]



=== TEST 3: Lua bridge - Lua chunk can error
--- valgrind
--- config
    location /t {
        wasm_call rewrite ngx_lua_tests test_lua_error;
        return 200;
    }
--- error_code: 500
--- error_log eval
[
    qr/\[error\] .*? lua user thread aborted: runtime error:.*? my error/,
    "stack traceback:"
]
--- no_error_log
[crit]



=== TEST 4: Lua bridge - Lua chunk can yield
--- config
    location /t {
        wasm_call rewrite ngx_lua_tests test_lua_sleep;
        return 200;
    }
--- error_log eval
[
    qr/\[notice\] .*? sleeping for 250ms/,
    qr/\[notice\] .*? sleeping for 250ms/
]
--- no_error_log
[crit]



=== TEST 5: Lua bridge - Lua chunk can error after yielding
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- http_config
    init_worker_by_lua_block {
        _G.sleep_before_error = true
    }
--- config
    location /t {
        wasm_call access ngx_lua_tests test_lua_error;
        echo ok;
    }
--- error_code: 500
--- error_log eval
[
    qr/\[error\] .*? lua user thread aborted: runtime error:.*? my error/,
    qr/\[notice\] .*? sleeping before error/
]
--- no_error_log
[crit]
