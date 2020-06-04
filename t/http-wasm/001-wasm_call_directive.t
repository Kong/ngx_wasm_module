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

=== TEST 1: wasm_call_log directive
--- main_config
    wasm {
        module hello $TEST_NGINX_HTML_DIR/hello.wat;
    }
--- config
    location /t {
        wasm_call_log hello get;
        return 200;
    }
--- user_files
>>> hello.wat
(module
  (func $get)
  (export "get" (func $get))
)
--- error_log
wasm: executing 1 call(s) in log phase
