# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

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
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
resp Transfer-Encoding: chunked.*
resp Connection: close.*
testing in "ResponseHeaders".*
resp :status: 200.*
resp Content-Type: text\/plain.*
resp Transfer-Encoding: chunked.*
resp Connection: close.*
resp Server: (nginx|openresty).*
resp Date: .*? GMT.*
testing in "Log".*
resp :status: 200.*
resp Content-Type: text\/plain.*
resp Transfer-Encoding: chunked.*
resp Connection: close.*
resp Server: (nginx|openresty).*
resp Date: .*? GMT.*/



=== TEST 2: proxy_wasm - get_http_response_headers() includes added headers
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/add_response_header \
                              value=Hello:world';

        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/add_response_header \
                              value=Hello:world';

        proxy_wasm hostcalls 'on=response_headers test=/t/log/response_headers';
        proxy_wasm hostcalls 'on=log test=/t/log/response_headers';
        return 200;
    }
--- ignore_response_body
--- grep_error_log eval: qr/(testing in|resp) .*/
--- grep_error_log_out eval
qr/testing in "RequestHeaders".*
testing in "ResponseHeaders".*
testing in "ResponseHeaders".*
resp :status: 200.*
resp Content-Type: text\/plain.*
resp Content-Length: 0.*
resp Connection: close.*
resp Hello: world.*
resp Server: (nginx|openresty).*
resp Date: .*? GMT.*
resp Hello: world.*
testing in "Log".*
resp :status: 200.*
resp Content-Type: text\/plain.*
resp Content-Length: 0.*
resp Connection: close.*
resp Hello: world.*
resp Server: (nginx|openresty).*
resp Date: .*? GMT.*
resp Hello: world.*/



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
    qr/on_response_headers, 106 headers/
]
--- no_error_log
[error]
[alert]
