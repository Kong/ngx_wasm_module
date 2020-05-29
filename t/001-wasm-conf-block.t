# vim:set ft= ts=4 sw=4 et fdm=marker:
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 2);

run_tests();

__DATA__

=== TEST 1: wasm{} configuration block
--- main_config
    wasm {}
--- no_error_log
[error]



=== TEST 2: wasm{} - missing
--- main_config
--- error_log
no "wasm" section in configuration
--- must_die



=== TEST 3: wasm{} - empty
--- SKIP: "use" is not required anymore
--- main_config
    wasm {}
--- error_log
missing "use" directive in wasm configuration
--- must_die



=== TEST 4: wasm{} - duplicated
--- main_config
    wasm {}
    wasm {}
--- error_log
"wasm" directive is duplicate
--- must_die



=== TEST 5: wasm{} - duplicated 'use' directive
--- main_config
    wasm {
        use wasmtime;
        use wasmtime;
    }
--- error_log
"use" directive is duplicate
--- must_die



=== TEST 6: wasm{} - invalid 'use' directive
--- main_config
    wasm {
        use foo;
    }
--- error_log
invalid wasm vm "foo"
--- must_die



=== TEST 7: wasm{} - default vm
--- main_config
    wasm {}
--- error_log eval
qr/\[notice\] .*? using the "wasmtime" wasm vm/



=== TEST 8: wasm{} use wasmtime vm
--- main_config
    wasm {
        use wasmtime;
    }
--- error_log eval
qr/\[notice\] .*? using the "wasmtime" wasm vm/
