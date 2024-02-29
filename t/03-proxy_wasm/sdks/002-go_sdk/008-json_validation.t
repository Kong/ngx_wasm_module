# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

skip_no_go_sdk();

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm Go SDK - json_validation example (fails without content-type)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: go_json_validation
--- config
    location /t {
        proxy_wasm go_json_validation '{ "requiredKeys": ["my_key"] }';
        echo ok;
    }
--- more_headers
content-type: text/html
--- request
POST /t

{ "my_key": "my_value" }
--- error_code: 403
--- response_body
content-type must be provided
--- error_log eval
qr/\[info\] .*? successfully loaded "go_json_validation"/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm Go SDK - json_validation example (fails with invalid payload)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: go_json_validation
--- config
    location /t {
        proxy_wasm go_json_validation '{ "requiredKeys": ["my_key"] }';
        echo ok;
    }
--- more_headers
content-type: application/json
--- request
POST /t

bla bla bla bla
--- error_code: 403
--- response_body
invalid payload
--- error_log eval
[
    qr/\[info\] .*? successfully loaded "go_json_validation"/,
    qr/\[error\] .*? body is not a valid json/,
]
--- no_error_log
[crit]



=== TEST 3: proxy_wasm Go SDK - json_validation example (fails with unknown key)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: go_json_validation
--- config
    location /t {
        proxy_wasm go_json_validation '{ "requiredKeys": ["my_key"] }';
        echo ok;
    }
--- more_headers
content-type: application/json
--- request
POST /t

{ "unknown_key": "unknown_value" }
--- error_code: 403
--- response_body
invalid payload
--- error_log eval
[
    qr/\[info\] .*? successfully loaded "go_json_validation"/,
    qr/\[error\] .*? required key \(my_key\) is missing/,
]
--- no_error_log
[crit]



=== TEST 4: proxy_wasm Go SDK - json_validation example (success)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: go_json_validation
--- config
    location /t {
        proxy_wasm go_json_validation '{ "requiredKeys": ["my_key"] }';
        echo ok;
    }
--- more_headers
content-type: application/json
--- request
POST /t

{ "my_key": "my_value" }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? successfully loaded "go_json_validation"/
--- no_error_log
[crit]
[error]
