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
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/response_headers';
        echo A;
    }

    location /t/B {
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/response_headers';
        echo B;
    }

    location /t {
        echo_location /t/A;
        echo_location /t/B;
        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_headers';
    }
--- ignore_response_body
--- grep_error_log eval: qr/\[wasm\] .*?(#\d+ entering "\S+"|resp\s).*?(?=\s+<)/
--- grep_error_log_out eval
qr/\[wasm\] .*? entering "RequestHeaders"
\[wasm\] resp Transfer-Encoding: chunked
\[wasm\] resp Connection: close
\[wasm\] .*? entering "ResponseHeaders"
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
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: hostcalls
--- config eval
qq^
    location /t {
^.
        (CORE::join "\n", map { "more_set_headers \"Header$_: value-$_\";" } 1..101).
qq^
        return 200;

        proxy_wasm hostcalls 'on=log \
                              test=/t/log/response_headers';
    }
^
--- ignore_response_body
--- error_log eval
[
    qr/\[warn\] .*? marshalled map truncated to 100 elements/,
    qr/\[debug\] .*? on_response_headers, 106 headers/
]
--- no_error_log
[error]
[alert]
