# vim:set ft= ts=4 sw=4 et fdm=marker:
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 2);

run_tests();

__DATA__

=== TEST 1: NGINX supports a wasm{} configuration block
--- main_config
    wasm {
        spec proxy_wasm;
        vm   wasmtime;
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
    wasm { spec proxy_wasm; vm wasmtime; }
    wasm { spec proxy_wasm; vm wasmtime; }
--- config
--- must_die
--- error_log
"wasm" directive is duplicate



=== TEST 4: duplicated 'vm' directive
--- main_config
    wasm {
        vm wasmtime;
        vm wasmtime;
    }
--- config
--- must_die
--- error_log
"vm" directive is duplicate



=== TEST 5: invalid 'vm' directive
--- main_config
    wasm {
        vm foo;
    }
--- config
--- must_die
--- error_log
invalid wasm vm "foo"



=== TEST 6: duplicated 'spec' directive
--- main_config
    wasm {
        spec proxy_wasm;
        spec proxy_wasm;
    }
--- config
--- must_die
--- error_log
"spec" directive is duplicate



=== TEST 7: invalid 'spec' directive
--- main_config
    wasm {
        spec foo;
    }
--- config
--- must_die
--- error_log
invalid wasm spec "foo"



=== TEST 8: vm = wasmtime + spec = proxy_wasm
--- main_config
    wasm {
        spec proxy_wasm;
        vm   wasmtime;
    }
--- config
--- error_log eval
qr/\[notice\] .*? using wasm spec "proxy_wasm" with vm "wasmtime"/
