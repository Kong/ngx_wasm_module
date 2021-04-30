# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 7);

add_block_preprocessor(sub {
    my $block = shift;

    if (!defined $block->no_error_log) {
        $block->set_value("no_error_log",
                          "[error]\n[crit]\n[emerg]\n[alert]");
    }
});

run_tests();

__DATA__

=== TEST 1: produce default response headers
should produce some of the default headers otherwise
injected (too late) by ngx_http_header_filter
--- skip_no_debug: 7
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        server_tokens on;
        wasm_call content ngx_rust_tests say_hello;
    }
--- response_headers_like
Content-Length: 10
Server: nginx\/\d+\.\d+\.\d+
Date: .*? GMT
Content-Type: text\/plain
--- ignore_response_body
--- grep_error_log eval: qr/wasm setting response header: .*?$/
--- grep_error_log_out eval
qr/wasm setting response header: "Content-Length: 10"
wasm setting response header: "Server: nginx\/\d+\.\d+\.\d+"
wasm setting response header: "Date: .*? GMT"
wasm setting response header: "Content-Type: text\/plain"
/
--- no_error_log
[error]



=== TEST 2: produce 'Server' response header with server_tokens=off
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        server_tokens off;
        wasm_call content ngx_rust_tests say_hello;
    }
--- response_headers_like
Server: nginx
--- response_body
hello say



=== TEST 3: produce 'Server' response header with server_tokens=build
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        server_tokens build;
        wasm_call content ngx_rust_tests say_hello;
    }
--- response_headers_like
Server: nginx\/\d+\.\d+\.\d+ \(ngx_wasm_module \[.*?\]\)
--- response_body
hello say



=== TEST 4: produce 'Content-Type' response header with charset suffix
--- wasm_modules: ngx_rust_tests
--- config
    error_log logs/error.log debug_core;

    location /t {
        charset 'UTF-8';
        wasm_call content ngx_rust_tests say_hello;
    }
--- response_headers_like
Content-Type: text\/plain; charset=UTF-8
--- response_body
hello say



=== TEST 5: produce 'Last-Modified' response header
--- skip_no_debug: 7
--- wasm_modules: ngx_rust_tests
--- config
    location /t {
        empty_gif;
        wasm_call log ngx_rust_tests log_notice_hello;
    }
--- response_headers_like
Last-Modified: .*? GMT
--- ignore_response_body
--- grep_error_log eval: qr/wasm setting response header: "Last-Modified: .*?"/
--- grep_error_log_out eval
qr/wasm setting response header: "Last-Modified: .*? GMT"/
