# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_no_go_sdk();

plan_tests(6);
run_tests();

__DATA__

=== TEST 1: proxy_wasm Go SDK - http_body example (echo)
--- wasm_modules: go_http_body
--- config
    location /t {
        proxy_wasm go_http_body 'echo';
    }
--- request
PUT /t

[original body]
--- response_body
[original body]
--- error_log eval
qr/\[info\] .*? successfully loaded "go_http_body"/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 2: proxy_wasm Go SDK - http_body example (prepend request body)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: go_http_body
--- config
    location /t {
        proxy_wasm go_http_body 'set';
        echo_request_body;
    }
--- request
PUT /t

[request body]
--- more_headers
buffer-operation: prepend
buffer-replace-at: request
--- response_headers
Transfer-Encoding: chunked
Content-Length:
--- response_body eval
qr/\[this is prepended body\]\[request body\]/
--- error_log eval
qr/\[info\] .*? successfully loaded "go_http_body"/
--- no_error_log
[error]



=== TEST 3: proxy_wasm Go SDK - http_body example (prepend response body)
--- wasm_modules: go_http_body
--- http_config eval
qq{
    upstream test_upstream {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location /t {
            return 200 '[response body]';
        }
    }
}
--- config
    location /t {
        proxy_wasm go_http_body 'set';
        proxy_pass http://test_upstream/t;
    }
--- request
PUT /t

[request body]
--- more_headers
buffer-operation: prepend
buffer-replace-at: response
--- response_body eval
qr/\[this is prepended body\]\[response body\]/
--- error_log eval
qr/\[info\] .*? successfully loaded "go_http_body"/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 4: proxy_wasm Go SDK - http_body example (append request body)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: go_http_body
--- config
    location /t {
        proxy_wasm go_http_body 'set';
        echo_request_body;
    }
--- request
PUT /t

[request body]
--- more_headers
buffer-operation: append
buffer-replace-at: request
--- response_body eval
qr/\[request body\]\[this is appended body\]/
--- error_log eval
qr/\[info\] .*? successfully loaded "go_http_body"/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 5: proxy_wasm Go SDK - http_body example (append response body)
--- wasm_modules: go_http_body
--- http_config eval
qq{
    upstream test_upstream {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location /t {
            return 200 '[response body]';
        }
    }
}
--- config
    location /t {
        proxy_wasm go_http_body 'set';
        proxy_pass http://test_upstream/t;
    }
--- request
PUT /t

[request body]
--- more_headers
buffer-operation: append
buffer-replace-at: response
--- response_body eval
qr/\[response body\]\[this is appended body\]/
--- error_log eval
qr/\[info\] .*? successfully loaded "go_http_body"/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 6: proxy_wasm Go SDK - http_body example (replace over a shorter request body)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: go_http_body
--- config
    location /t {
        proxy_wasm go_http_body 'set';
        echo_request_body;
    }
--- request
PUT /t

[request body]
--- more_headers
buffer-operation: replace
buffer-replace-at: request
--- response_body eval
qr/\[this is replaced body\]/
--- error_log eval
qr/\[info\] .*? successfully loaded "go_http_body"/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 7: proxy_wasm Go SDK - http_body example (replace over a shorter response body)
--- wasm_modules: go_http_body
--- http_config eval
qq{
    upstream test_upstream {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location /t {
            return 200 '[response body]';
        }
    }
}
--- config
    location /t {
        proxy_wasm go_http_body 'set';
        proxy_pass http://test_upstream/t;
    }
--- request
PUT /t

[looooooooooooooooooooonger request body]
--- more_headers
buffer-operation: replace
buffer-replace-at: response
--- response_body eval
qr/\[this is replaced body\]/
--- error_log eval
qr/\[info\] .*? successfully loaded "go_http_body"/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 8: proxy_wasm Go SDK - http_body example (replace over a longer request body)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: go_http_body
--- config
    location /t {
        proxy_wasm go_http_body 'set';
        echo_request_body;
    }
--- request
PUT /t

[looooooooooooooooooooooooooooonger request body]
--- more_headers
buffer-operation: replace
buffer-replace-at: request
--- response_body eval
qr/\[this is replaced body\]/
--- error_log eval
qr/\[info\] .*? successfully loaded "go_http_body"/
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 9: proxy_wasm Go SDK - http_body example (replace over a longer response body)
--- wasm_modules: go_http_body
--- http_config eval
qq{
    upstream test_upstream {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location /t {
            return 200 '[looooooooooooooooooooonger response body]';
        }
    }
}
--- config
    location /t {
        proxy_wasm go_http_body 'set';
        proxy_pass http://test_upstream/t;
    }
--- request
PUT /t

[request body]
--- more_headers
buffer-operation: replace
buffer-replace-at: response
--- response_body eval
qr/\[this is replaced body\]/
--- error_log eval
qr/\[info\] .*? successfully loaded "go_http_body"/
--- no_error_log
[error]
[crit]
[emerg]
