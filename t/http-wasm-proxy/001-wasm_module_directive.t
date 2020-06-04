# vim:set ft= ts=4 sw=4 et fdm=marker:
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 3);

add_block_preprocessor(sub {
    my $block = shift;

    if (!defined $block->no_error_log) {
        $block->set_value("no_error_log", "[error]\n[emerg]");
    }
});

run_tests();

__DATA__

=== TEST 1: proxy_wasm_module directive
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- config
    location /t {
        proxy_wasm_module hello;
        return 200;
    }
--- user_files
>>> hello.wat
(module)



=== TEST 2: proxy_wasm_module directive: invalid module name
--- config
    location /t {
        proxy_wasm_module '';
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? invalid module name ""/
--- no_error_log
[error]
--- must_die



=== TEST 3: proxy_wasm_module directive: no such module
--- main_config
    wasm {}
--- config
    location /t {
        proxy_wasm_module hello;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? no "hello" module defined/
--- no_error_log
[error]
--- must_die



=== TEST 4: proxy_wasm_module directive: no wasm{} configuration block
--- config
    location /t {
        proxy_wasm_module hello;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? no "hello" module defined/
--- no_error_log
[error]
--- must_die
