# vim:set ft= ts=4 sw=4 et fdm=marker:
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 2);

run_tests();

__DATA__

=== TEST 1: NGINX supports a wasm{} configuration block
--- main_config
    wasm {
        use wasmtime;
    }
--- config
    location /t {
        wasm_module '/home/chasum/code/wasm/hello-world/target/wasm32-unknown-unknown/debug/hello.wasm';
        return 200;
    }
--- request
GET /t
--- no_error_log
[error]
