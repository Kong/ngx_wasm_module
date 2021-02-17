# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 3);

add_block_preprocessor(sub {
    my $block = shift;
    my $main_config = <<_EOC_;
        wasm {
            module ngx_rust_tests $t::TestWasm::crates/ngx_rust_tests.wasm;
        }
_EOC_

    if (!defined $block->main_config) {
        $block->set_value("main_config", $main_config);
    }
});

run_tests();

__DATA__

=== TEST 1: say: produce response in 'rewrite' phase
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests say_hello;
    }
--- response_body
hello say
--- no_error_log
[error]



=== TEST 2: say: produce response in 'content' phase
--- config
    location /t {
        wasm_call content ngx_rust_tests say_hello;
    }
--- response_body
hello say
--- no_error_log
[error]



=== TEST 3: say: 'log' phase
--- SKIP
--- config
    location /t {
        wasm_call log ngx_rust_tests say_hello;
    }
--- response_body
--- no_error_log
[error]
