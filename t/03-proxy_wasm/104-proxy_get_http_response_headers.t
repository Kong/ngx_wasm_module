# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 5);

add_block_preprocessor(sub {
    my $block = shift;

    if (!defined $block->no_error_log) {
        $block->set_value("no_error_log",
                          "[error]\n[crit]\n[emerg\]");
    }
});

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_http_response_headers() x on_phases
should produce no results in: on_request_headers
should produce results in: on_response_headers, on_log
should only include default produced headers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t/A {
        proxy_wasm hostcalls on_phase=http_request_headers;
        echo A;
    }

    location /t/B {
        proxy_wasm hostcalls on_phase=http_response_headers;
        echo B;
    }

    location /t {
        echo_location /t/A;
        echo_location /t/B;
        proxy_wasm hostcalls on_phase=log;
    }
--- more_headers
PWM-Test-Case: /t/log/response_headers
--- ignore_response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpRequestHeaders"
\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] resp Server: nginx.*?
\[wasm\] resp Date: .*? GMT
\[wasm\] resp Content-Type: text\/plain
\[wasm\] .*? entering "Log"
\[wasm\] resp Server: nginx.*?
\[wasm\] resp Date: .*? GMT
\[wasm\] resp Content-Type: text\/plain/



=== TEST 2: proxy_wasm - get_http_response_headers() includes added headers
--- SKIP: TODO
--- wasm_modules: hostcalls



=== TEST 3: proxy_wasm - get_http_response_headers() too many headers
should truncate > 100 headers
--- SKIP: TODO
--- wasm_modules: hostcalls
