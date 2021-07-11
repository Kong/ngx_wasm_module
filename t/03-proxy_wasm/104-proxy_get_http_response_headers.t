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
                          "[error]\n[crit]\n[alert\]");
    }
});

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_http_response_headers() x on_phases
should always include as many headers as it knows will be in the
response
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t/A {
        proxy_wasm hostcalls 'on_phase=http_request_headers \
                              test_case=/t/log/response_headers';
        echo A;
    }

    location /t/B {
        proxy_wasm hostcalls 'on_phase=http_response_headers \
                              test_case=/t/log/response_headers';
        echo B;
    }

    location /t {
        echo_location /t/A;
        echo_location /t/B;
        proxy_wasm hostcalls 'on_phase=log \
                              test_case=/t/log/response_headers';
    }
--- ignore_response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "HttpRequestHeaders"
\[wasm\] resp Transfer-Encoding: chunked
\[wasm\] resp Connection: close
\[wasm\] .*? entering "HttpResponseHeaders"
\[wasm\] resp Content-Type: text\/plain
\[wasm\] resp Transfer-Encoding: chunked
\[wasm\] resp Connection: close
\[wasm\] resp Server: nginx.*?
\[wasm\] resp Date: .*? GMT
\[wasm\] .*? entering "Log"
\[wasm\] resp Content-Type: text\/plain
\[wasm\] resp Transfer-Encoding: chunked
\[wasm\] resp Connection: close
\[wasm\] resp Server: nginx.*?
\[wasm\] resp Date: .*? GMT/



=== TEST 2: proxy_wasm - get_http_response_headers() includes added headers
--- SKIP: TODO
--- wasm_modules: hostcalls



=== TEST 3: proxy_wasm - get_http_response_headers() too many headers
should truncate > 100 headers
--- SKIP: TODO
--- wasm_modules: hostcalls
