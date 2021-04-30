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



=== TEST 3: say: produce response in 'header_filter' phase
--- load_nginx_modules: ngx_http_echo_module ngx_http_headers_more_filter_module
--- config
    location /t {
        # force the Content-Length header value since say
        # will set it only once otherwise
        more_set_headers 'Content-Length: 20';
        wasm_call content ngx_rust_tests say_hello;
        wasm_call header_filter ngx_rust_tests say_hello;
    }
--- response_body
hello say
hello say
--- no_error_log
[error]



=== TEST 4: say: 'log' phase
should produce a trap
TODO: test backtrace
--- config
    location /t {
        return 200;
        wasm_call log ngx_rust_tests say_hello;
    }
--- ignore_response_body
--- grep_error_log eval: qr/\[error\] .*?$/
--- grep_error_log_out eval
qr/\[wasm\] bad host usage: response already sent/
--- no_error_log
[crit]
