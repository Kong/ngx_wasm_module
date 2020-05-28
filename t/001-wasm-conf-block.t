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
--- no_error_log
[error]



=== TEST 2: empty wasm block
--- main_config
    wasm {}
--- config
--- must_die
--- error_log
missing "vm" directive in wasm configuration



=== TEST 3: detects duplicated wasm{} blocks
--- main_config
    wasm { use wasmtime; }
    wasm { use wasmtime; }
--- config
--- must_die
--- error_log
"wasm" directive is duplicate



=== TEST 4: duplicated 'vm' directive
--- main_config
    wasm {
        use wasmtime;
        use wasmtime;
    }
--- config
--- must_die
--- error_log
"use" directive is duplicate



=== TEST 5: invalid 'vm' directive
--- main_config
    wasm {
        use foo;
    }
--- config
--- must_die
--- error_log
invalid wasm vm "foo"



=== TEST 6: use wasmtime
--- ONLY
--- main_config
    wasm {
        use wasmtime;
    }
--- config
--- error_log eval
qr/\[notice\] .*? using wasm vm "wasmtime"/
