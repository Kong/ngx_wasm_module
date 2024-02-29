# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

BEGIN {
    $ENV{TEST_NGINX_EVENT_TYPE} = 'poll';
}

use strict;
use lib '.';
use t::TestWasmX;

add_block_preprocessor(sub {
    my $block = shift;

    if (!defined $block->no_error_log) {
        $block->set_value("no_error_log", "[error]\n[crit]");
    }

    if (!defined $block->load_nginx_modules) {
        $block->set_value("load_nginx_modules",
                          "ngx_http_echo_module " .
                          "ngx_http_headers_more_filter_module");
    }

    if (!defined $block->wasm_modules) {
        $block->set_value("wasm_modules", "on_phases");
    }
});

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - on_response_body, no buffering
--- config
    location /t {
        echo -n 'a\n';
        echo_flush;
        echo -n 'bb\n';
        echo_flush;
        echo -n 'ccc\n';
        echo_flush;
        echo -n 'dddd\n';
        proxy_wasm on_phases;
    }
--- response_body
a
bb
ccc
dddd
--- grep_error_log eval: qr/on_response_body, .*?eof: (true|false)/
--- grep_error_log_out
on_response_body, 2 bytes, eof: false
on_response_body, 3 bytes, eof: false
on_response_body, 4 bytes, eof: false
on_response_body, 5 bytes, eof: false
on_response_body, 0 bytes, eof: true



=== TEST 2: proxy_wasm - on_response_body buffering chunks, default buffers
--- config
    location /t {
        wasm_response_body_buffers 4 32;
        echo -n 'a\n';
        echo_flush;
        echo -n 'bb\n';
        echo_flush;
        echo -n 'ccc\n';
        echo_flush;
        echo -n 'dddd\n';
        proxy_wasm on_phases 'pause_on=response_body';
    }
--- response_body
a
bb
ccc
dddd
--- grep_error_log eval: qr/on_response_body, .*?eof: (true|false)/
--- grep_error_log_out
on_response_body, 2 bytes, eof: false
on_response_body, 14 bytes, eof: true



=== TEST 3: proxy_wasm - on_response_body buffering chunks, buffers too small for body
--- config
    location /t {
        wasm_response_body_buffers 3 2;
        echo -n 'a\n';
        echo_flush;
        echo -n 'bb\n';
        echo_flush;
        echo -n 'ccc\n';
        echo_flush;
        echo -n 'dddd\n';
        proxy_wasm on_phases 'pause_on=response_body';
    }
--- response_body
a
bb
ccc
dddd
--- grep_error_log eval: qr/on_response_body, .*?eof: (true|false)/
--- grep_error_log_out
on_response_body, 2 bytes, eof: false
on_response_body, 9 bytes, eof: false
on_response_body, 5 bytes, eof: false
on_response_body, 0 bytes, eof: true



=== TEST 4: proxy_wasm - on_response_body buffering chunks, buffers larger than body
--- config
    location /t {
        wasm_response_body_buffers 4 32;
        echo -n 'a\n';
        echo_flush;
        echo -n 'bb\n';
        echo_flush;
        echo -n 'ccc\n';
        echo_flush;
        echo -n 'dddd\n';
        proxy_wasm on_phases 'pause_on=response_body';
    }
--- response_body
a
bb
ccc
dddd
--- grep_error_log eval: qr/on_response_body, .*?eof: (true|false)/
--- grep_error_log_out
on_response_body, 2 bytes, eof: false
on_response_body, 14 bytes, eof: true



=== TEST 5: proxy_wasm - on_response_body buffering chunks, buffers same size as body
--- config
    location /t {
        wasm_response_body_buffers 1 14;
        echo -n 'a\n';
        echo_flush;
        echo -n 'bb\n';
        echo_flush;
        echo -n 'ccc\n';
        echo_flush;
        echo -n 'dddd\n';
        proxy_wasm on_phases 'pause_on=response_body';
    }
--- response_body
a
bb
ccc
dddd
--- grep_error_log eval: qr/on_response_body, .*?eof: (true|false)/
--- grep_error_log_out
on_response_body, 2 bytes, eof: false
on_response_body, 14 bytes, eof: true



=== TEST 6: proxy_wasm - on_response_body buffering chunks, buffer of size 1 for larger body
--- config
    location /t {
        wasm_response_body_buffers 1 1;
        echo -n 'a\n';
        echo_flush;
        echo -n 'bb\n';
        echo_flush;
        echo -n 'ccc\n';
        echo_flush;
        echo -n 'dddd\n';
        proxy_wasm on_phases 'pause_on=response_body';
    }
--- response_body
a
bb
ccc
dddd
--- grep_error_log eval: qr/on_response_body, .*?eof: (true|false)/
--- grep_error_log_out
on_response_body, 2 bytes, eof: false
on_response_body, 2 bytes, eof: false
on_response_body, 3 bytes, eof: false
on_response_body, 4 bytes, eof: false
on_response_body, 5 bytes, eof: false
on_response_body, 0 bytes, eof: true



=== TEST 7: proxy_wasm - on_response_body buffering chunks, buffers same size as body
--- config
    location /t {
        wasm_response_body_buffers 1 14;
        # unlike 'echo -n', this format creates 2 chained buffers for each 'echo',
        # the 2nd one being the newline buffer.
        echo 'a';
        echo_flush;
        echo 'bb';
        echo_flush;
        echo 'ccc';
        echo_flush;
        echo 'dddd';
        proxy_wasm on_phases 'pause_on=response_body';
    }
--- response_body
a
bb
ccc
dddd
--- grep_error_log eval: qr/on_response_body, .*?eof: (true|false)/
--- grep_error_log_out
on_response_body, 2 bytes, eof: false
on_response_body, 14 bytes, eof: true



=== TEST 8: proxy_wasm - on_response_body buffering chunks, buffer of size 1 for larger body
--- config
    location /t {
        wasm_response_body_buffers 1 1;
        # unlike 'echo -n', this format creates 2 chained buffers for each 'echo',
        # the 2nd one being the newline buffer.
        echo 'a';
        echo_flush;
        echo 'bb';
        echo_flush;
        echo 'ccc';
        echo_flush;
        echo 'dddd';
        proxy_wasm on_phases 'pause_on=response_body';
    }
--- response_body
a
bb
ccc
dddd
--- grep_error_log eval: qr/on_response_body, .*?eof: (true|false)/
--- grep_error_log_out
on_response_body, 2 bytes, eof: false
on_response_body, 2 bytes, eof: false
on_response_body, 3 bytes, eof: false
on_response_body, 4 bytes, eof: false
on_response_body, 5 bytes, eof: false
on_response_body, 0 bytes, eof: true



=== TEST 9: proxy_wasm - on_response_body buffering chunks, buffers smaller than subrequests bodies
--- config
    location /a {
        internal;
        echo a;
    }

    location /b {
        internal;
        echo bb;
    }

    location /c {
        internal;
        echo ccc;
    }

    location /d {
        internal;
        echo dddd;
    }

    location /t {
        wasm_response_body_buffers 1 1;
        echo_subrequest GET '/a';
        echo_subrequest GET '/b';
        echo_subrequest GET '/c';
        echo_subrequest GET '/d';
        proxy_wasm on_phases 'pause_on=response_body';
    }
--- response_body
a
bb
ccc
dddd
--- grep_error_log eval: qr/on_response_body, .*?eof: (true|false)/
--- grep_error_log_out
on_response_body, 2 bytes, eof: false
on_response_body, 2 bytes, eof: false
on_response_body, 3 bytes, eof: false
on_response_body, 4 bytes, eof: false
on_response_body, 5 bytes, eof: false
on_response_body, 0 bytes, eof: true



=== TEST 10: proxy_wasm - on_response_body buffering with proxy_pass (buffers too small for body)
Clear Server header or else different build modes produce different
bufferring results (no-pool/openresty)
--- http_config eval
qq{
    upstream test_upstream {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location / {
            more_clear_headers 'Server';
            echo_duplicate 128 'a';
            echo_flush;
            echo_duplicate 128 'b';
        }
    }
}
--- config
    location /t {
        wasm_response_body_buffers 1 64;

        proxy_buffer_size 256;
        proxy_buffers 3 128;
        proxy_busy_buffers_size 256;

        proxy_pass http://test_upstream/;
        proxy_wasm on_phases 'pause_on=response_body';
    }
--- response_body_like: a{128}b{128}
--- grep_error_log eval: qr/on_response_body, .*?eof: (true|false)/
--- grep_error_log_out
on_response_body, 155 bytes, eof: false
on_response_body, 155 bytes, eof: false
on_response_body, 101 bytes, eof: false
on_response_body, 0 bytes, eof: true



=== TEST 11: proxy_wasm - on_response_body buffering with proxy_pass (buffers larger than body)
Clear Server header or else different build modes produce different
bufferring results (no-pool/openresty)
--- http_config eval
qq{
    upstream test_upstream {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location / {
            more_clear_headers 'Server';
            echo_duplicate 128 "a";
            echo_flush;
            echo_duplicate 128 "b";
            echo_flush;
            echo_duplicate 128 "c";
            echo_flush;
            echo_duplicate 128 "d";
        }
    }
}
--- config
    location /t {
        wasm_response_body_buffers 6 256;

        proxy_buffer_size 256;
        proxy_buffers 3 128;
        proxy_busy_buffers_size 256;

        proxy_pass http://test_upstream/;
        proxy_wasm on_phases 'pause_on=response_body';
    }
--- response_body_like: a{128}b{128}c{128}d{128}
--- grep_error_log eval: qr/on_response_body, .*?eof: (true|false)/
--- grep_error_log_out
on_response_body, 155 bytes, eof: false
on_response_body, 512 bytes, eof: true



=== TEST 12: proxy_wasm - on_response_body buffering with proxy_pass (buffers same size as body)
Clear Server header or else different build modes produce different
bufferring results (no-pool/openresty)
--- http_config eval
qq{
    upstream test_upstream {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location / {
            more_clear_headers 'Server';
            echo_duplicate 128 "a";
            echo_flush;
            echo_duplicate 128 "b";
            echo_flush;
            echo_duplicate 128 "c";
            echo_flush;
            echo_duplicate 128 "d";
        }
    }
}
--- config
    location /t {
        wasm_response_body_buffers 4 128;

        proxy_buffer_size 256;
        proxy_buffers 3 128;
        proxy_busy_buffers_size 256;

        proxy_pass http://test_upstream/;
        proxy_wasm on_phases 'pause_on=response_body';
    }
--- response_body_like: a{128}b{128}c{128}d{128}
--- grep_error_log eval: qr/on_response_body, .*?eof: (true|false)/
--- grep_error_log_out
on_response_body, 155 bytes, eof: false
on_response_body, 512 bytes, eof: true



=== TEST 13: proxy_wasm - on_response_body buffering, get_http_response_body() when buffers larger than body
--- valgrind
--- config
    location /t {
        wasm_response_body_buffers 4 32;
        echo -n 'a\n';
        echo_flush;
        echo -n 'bb\n';
        echo_flush;
        echo -n 'ccc\n';
        echo_flush;
        echo -n 'dddd\n';
        proxy_wasm on_phases 'pause_on=response_body';
        proxy_wasm on_phases 'log_response_body=true';
    }
--- response_body
a
bb
ccc
dddd
--- grep_error_log eval: qr/on_response_body, .*?eof: (true|false)/
--- grep_error_log_out
on_response_body, 2 bytes, eof: false
on_response_body, 14 bytes, eof: true
on_response_body, 14 bytes, eof: true
--- error_log
response body chunk: "a\nbb\nccc\ndddd\n"
--- no_error_log
[error]



=== TEST 14: proxy_wasm - on_response_body buffering, get_http_response_body() when buffers too small for body
--- valgrind
--- config
    location /t {
        wasm_response_body_buffers 3 2;
        echo -n 'a\n';
        echo_flush;
        echo -n 'bb\n';
        echo_flush;
        echo -n 'ccc\n';
        echo_flush;
        echo -n 'dddd\n';
        proxy_wasm on_phases 'pause_on=response_body';
        proxy_wasm on_phases 'log_response_body=true';
    }
--- response_body
a
bb
ccc
dddd
--- grep_error_log eval: qr/on_response_body, .*?eof: (true|false)/
--- grep_error_log_out
on_response_body, 2 bytes, eof: false
on_response_body, 9 bytes, eof: false
on_response_body, 9 bytes, eof: false
on_response_body, 5 bytes, eof: false
on_response_body, 5 bytes, eof: false
on_response_body, 0 bytes, eof: true
on_response_body, 0 bytes, eof: true
--- error_log
response body chunk: "a\nbb\nccc\n"
response body chunk: "dddd\n"
--- no_error_log



=== TEST 15: proxy_wasm - on_response_body buffering, then ask for buffering again
--- config
    location /t {
        wasm_response_body_buffers 3 32;
        echo -n 'a\n';
        echo_flush;
        echo -n 'bb\n';
        echo_flush;
        echo -n 'ccc\n';
        echo_flush;
        echo -n 'dddd\n';
        proxy_wasm on_phases 'pause_on=response_body pause_times=2';
        proxy_wasm on_phases 'log_response_body=true';
    }
--- response_body
a
bb
ccc
dddd
--- grep_error_log eval: qr/on_response_body, .*?eof: (true|false)/
--- grep_error_log_out
on_response_body, 2 bytes, eof: false
on_response_body, 14 bytes, eof: true
on_response_body, 14 bytes, eof: true
--- error_log eval
[
    "response body chunk: \"a\\nbb\\nccc\\ndddd\\n\"",
    qr/\[error\] .*? invalid "on_response_body" return action \(PAUSE\): response body buffering already requested/
]
--- no_error_log
