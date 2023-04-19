# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm::Lua;

skip_no_openresty();

plan tests => repeat_each() * (blocks() * 4);

add_block_preprocessor(sub {
    my $block = shift;
    if (!defined $block->wasm_modules) {
        $block->set_value("wasm_modules", "ngx_lua_tests");
    }
});

run_tests();

__DATA__

=== TEST 1: Lua bridge - Lua chunk arguments and return values
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
