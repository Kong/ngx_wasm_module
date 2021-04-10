# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

#repeat_each(2);

plan tests => repeat_each() * (blocks() * 7);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - on_request_headers
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module on_http_phases $TEST_NGINX_CRATES_DIR/on_http_phases.wasm;
    }
--- config
    location /t {
        proxy_wasm on_http_phases;
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? \[wasm\] #\d+ on_request_headers, 2 headers/
--- no_error_log
[error]
[emerg]
[alert]
[crit]



=== TEST 2: proxy_wasm - on_response_headers
should log 0 response headers (TODO: include default headers)
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module on_http_phases $TEST_NGINX_CRATES_DIR/on_http_phases.wasm;
    }
--- config
    location /t {
        proxy_wasm on_http_phases;
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? \[wasm\] #\d+ on_response_headers, 0 headers/
--- no_error_log
[error]
[emerg]
[alert]
[crit]



=== TEST 3: proxy_wasm - on_done
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module on_http_phases $TEST_NGINX_CRATES_DIR/on_http_phases.wasm;
    }
--- config
    location /t {
        proxy_wasm on_http_phases;
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? \[wasm\] #\d+ on_done/
--- no_error_log
[error]
[emerg]
[alert]
[crit]



=== TEST 4: proxy_wasm - missing default content handler
should cause HTTP 404 from static module (default content handler)
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module on_http_phases $TEST_NGINX_CRATES_DIR/on_http_phases.wasm;
    }
--- config
    location /t {
        proxy_wasm on_http_phases;
        echo_status 201;
    }
--- error_code: 404
--- response_body eval
qr/404 Not Found/
--- error_log eval
[
    qr/\[error\] .*? open\(\) \".*?\" failed/,
    qr/\[info\] .*? \[wasm\] #\d+ on_request_headers, 2 headers/,
    qr/\[info\] .*? \[wasm\] #\d+ on_response_headers, 0 headers/,
    qr/\[info\] .*? \[wasm\] #\d+ on_done/
]
--- no_error_log
[crit]



=== TEST 5: proxy_wasm - with 'return' (rewrite)
should produce a response in and of itself, proxy_wasm wraps around
--- main_config
    wasm {
        module on_http_phases $TEST_NGINX_CRATES_DIR/on_http_phases.wasm;
    }
--- config
    location /t {
        proxy_wasm on_http_phases;
        return 201;
    }
--- error_code: 201
--- response_body
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] #\d+ on_request_headers, 2 headers/,
    qr/\[info\] .*? \[wasm\] #\d+ on_response_headers, 0 headers/,
    qr/\[info\] .*? \[wasm\] #\d+ on_done/
]
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm - with 'proxy_pass' (content)
should produce a response from proxy_pass, proxy_wasm wraps around
--- main_config
    wasm {
        module on_http_phases $TEST_NGINX_CRATES_DIR/on_http_phases.wasm;
    }
--- http_config eval
qq{
    upstream test_upstream {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location / {
            return 201;
        }
    }
}
--- config
    location /t {
        proxy_wasm on_http_phases;
        proxy_pass http://test_upstream/;
    }
--- error_code: 201
--- response_body
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] #\d+ on_request_headers, 2 headers/,
    qr/\[info\] .*? \[wasm\] #\d+ on_response_headers, 0 headers/,
    qr/\[info\] .*? \[wasm\] #\d+ on_done/
]
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - as subrequest
should not execute a log phase
--- load_nginx_modules: ngx_http_echo_module
--- main_config
    wasm {
        module on_http_phases $TEST_NGINX_CRATES_DIR/on_http_phases.wasm;
    }
--- config
    location /subrequest {
        internal;
        proxy_wasm on_http_phases;
        return 201;
    }

    location /t {
        echo_subrequest GET '/subrequest';
    }
--- error_code: 200
--- response_body
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] #\d+ on_request_headers, 2 headers/,
    qr/\[info\] .*? \[wasm\] #\d+ on_response_headers, 0 headers/,
]
--- no_error_log
on_done
[error]
[crit]
