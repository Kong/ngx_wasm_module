# vim:set ft= ts=4 sw=4 et fdm=marker:
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 3);

add_block_preprocessor(sub {
    my $block = shift;

    if (!defined $block->no_error_log) {
        $block->set_value("no_error_log", "[error]");
    }
});

run_tests();

__DATA__

=== TEST 1: wasm{} configuration block
--- main_config
    wasm {}
--- no_error_log
[error]



=== TEST 2: wasm{} - missing block
--- main_config
--- error_log
no "wasm" section in configuration
--- must_die



=== TEST 3: wasm{} - duplicated block
--- main_config
    wasm {}
    wasm {}
--- error_log
"wasm" directive is duplicate
--- must_die



=== TEST 4: wasm{} - use directive
--- main_config
    wasm {
        use wasmtime;
    }
--- error_log eval
qr/\[notice\] .*? using the "wasmtime" wasm vm/



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



=== TEST 7: wasm{} - default vm (no 'use' directive)
--- main_config
    wasm {}
--- error_log eval
qr/\[notice\] .*? using the "wasmtime" wasm vm/



=== TEST 8: wasm{} - module directive
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- user_files
>>> hello.wat
(module)
--- error_log eval
qr/\[notice\] .*? \[wasm\] loading module "hello" from ".*?hello\.wat"/



=== TEST 9: wasm{} - multiple module directives
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
        module world $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- user_files
>>> hello.wat
(module)
--- error_log eval
[qr/\[notice\] .*? \[wasm\] loading module "hello" from ".*?hello\.wat"/,
qr/\[notice\] .*? \[wasm\] loading module "world" from ".*?hello\.wat"/]



=== TEST 10: wasm{} - invalid 'module' directive (no name)
--- main_config
    wasm {
        module $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- error_log eval
qr/\[emerg\] .*? invalid number of arguments in "module" directive/
--- must_die



=== TEST 11: wasm{} - invalid 'module' directive (no path)
--- main_config
    wasm {
        module hello;
    }
--- error_log eval
qr/\[emerg\] .*? invalid number of arguments in "module" directive/
--- must_die



=== TEST 12: wasm{} - invalid 'module' directive (invalid name)
--- main_config
    wasm {
        module '' $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- error_log eval
qr/\[emerg\] .*? invalid module name ""/
--- must_die



=== TEST 13: wasm{} - invalid 'module' directive (invalid path)
--- main_config
    wasm {
        module hello '';
    }
--- error_log eval
qr/\[emerg\] .*? invalid module path ""/
--- must_die



=== TEST 14: wasm{} - invalid 'module' directive (already defined)
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- user_files
>>> hello.wat
(module)
--- error_log eval
qr/\[emerg\] .*? module "hello" already defined/
--- must_die



=== TEST 15: wasm{} - invalid 'module' directive (no such path)
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/none.wat;
    }
--- error_log eval
qr/\[emerg\] .*? \[wasm\] failed to load module "hello" from ".*?none\.wat" \(2: No such file or directory\)/
--- no_error_log
[error]
--- must_die



=== TEST 16: wasm{} - invalid 'module' directive (invalid module)
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- user_files
>>> hello.wat
--- error_log eval
qr/\[emerg\] .*? \[wasm\] failed to load module "hello" from ".*?hello\.wat" \(wasmtime error: expected at least one module field/
--- no_error_log
[error]
--- must_die
