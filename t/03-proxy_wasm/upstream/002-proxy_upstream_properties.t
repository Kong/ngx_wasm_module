# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

our $upstream_properties = join(',', qw(
    upstream.address
    upstream.status
    upstream.connect_time
    upstream.header_time
    upstream.response_time
));

plan tests => 36;
run_tests();

__DATA__

=== TEST 1: get upstream.* properties not found on non-upstream_info phases
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "ok";
        }
    }
    upstream test_upstream {
        server 127.0.0.1:8891;
        wasm_upstream_select;
    }
--- config eval
my $phases = CORE::join(',', qw(
    request_headers
    request_body
    response_headers
    response_body
));

qq{
    location /t {
        proxy_wasm hostcalls 'on=$phases \
                              test=/t/log/properties \
                              name=$::upstream_properties';
        proxy_pass http://test_upstream/;
    }
}
--- request
POST /t
hello
--- error_code: 200
--- response_body
ok
--- grep_error_log eval: qr/property not found: upstream\.\w+ at \w+/
--- grep_error_log_out eval
my $checks;
my @phases = qw(
    RequestHeaders
    RequestBody
    ResponseHeaders
    ResponseBody
);

foreach my $phase (@phases) {
    foreach my $var (split(',', $::upstream_properties)) {
        $checks .= "property not found: $var at $phase\n";
    }
}

qr/$checks/
--- no_error_log
[error]
[crit]



=== TEST 2: get upstream.* properties in on_upstream_info
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "ok";
        }
    }
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/log/properties on=upstream_info name=$::upstream_properties';
        proxy_pass http://test_upstream/;
    }
}
--- error_code: 200
--- response_body
ok
--- grep_error_log eval: qr/upstream\.[_a-z]+: .+ at UpstreamInfo/
--- grep_error_log_out eval
qr/upstream\.address: 127\.0\.0\.1:8891 at UpstreamInfo
upstream\.status: 200 at UpstreamInfo
upstream\.connect_time: \d+ at UpstreamInfo
upstream\.header_time: \d+ at UpstreamInfo
upstream\.response_time: \d+ at UpstreamInfo/
--- no_error_log
[error]
[crit]



=== TEST 3: get upstream.* properties change for each attempt
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            return 500;
        }
    }
    server {
        listen       8892;
        location / {
            return 404;
        }
    }
    server {
        listen       8893;
        location / {
            echo "ok";
        }
    }
    proxy_next_upstream_tries 3;
    proxy_next_upstream http_500 http_404;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstreams on=upstream_select upstreams=127.0.0.1:8891,127.0.0.1:8892,127.0.0.1:8893';
        proxy_wasm hostcalls 'test=/t/log/properties on=upstream_info name=upstream.address,upstream.status';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- grep_error_log eval: qr/upstream\.[_a-z]+: .+ at UpstreamInfo/
--- grep_error_log_out
upstream.address: 127.0.0.1:8891 at UpstreamInfo
upstream.status: 500 at UpstreamInfo
upstream.address: 127.0.0.1:8892 at UpstreamInfo
upstream.status: 404 at UpstreamInfo
upstream.address: 127.0.0.1:8893 at UpstreamInfo
upstream.status: 200 at UpstreamInfo
--- no_error_log
[emerg]



=== TEST 4: upstream times reflect delayed response
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo_sleep 0.1;
            echo "ok";
        }
    }
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/log/properties on=upstream_info name=upstream.connect_time,upstream.header_time,upstream.response_time';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- grep_error_log eval: qr/upstream\.[_a-z]+: .+ at UpstreamInfo/
--- grep_error_log_out eval
qr/upstream\.connect_time: \d{1,2} at UpstreamInfo
upstream\.header_time: \d{3,} at UpstreamInfo
upstream\.response_time: \d{3,} at UpstreamInfo/
--- no_error_log
[emerg]



=== TEST 5: upstream times not found on connection refused
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=9999';
        proxy_wasm hostcalls 'test=/t/log/properties on=upstream_info name=upstream.connect_time,upstream.header_time,upstream.response_time';
        proxy_pass http://test_upstream/;
    }
--- error_code: 502
--- error_log
property not found: upstream.connect_time
property not found: upstream.header_time
property not found: upstream.response_time
--- no_error_log
[emerg]



=== TEST 6: upstream times not found on connection timeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    proxy_connect_timeout 1ms;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=8.8.8.8 port=12345';
        proxy_wasm hostcalls 'test=/t/log/properties on=upstream_info name=upstream.connect_time,upstream.header_time,upstream.response_time';
        proxy_pass http://test_upstream/;
    }
--- error_code: 504
--- error_log
property not found: upstream.connect_time
property not found: upstream.header_time
property not found: upstream.response_time
--- no_error_log
[emerg]



=== TEST 7: connect_time available, header/response times not found on response timeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo_sleep 2;
            echo "ok";
        }
    }
    proxy_read_timeout 1s;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/log/properties on=upstream_info name=upstream.connect_time,upstream.header_time,upstream.response_time';
        proxy_pass http://test_upstream/;
    }
--- error_code: 504
--- error_log
property not found: upstream.header_time
property not found: upstream.response_time
--- no_error_log
property not found: upstream.connect_time
[emerg]



=== TEST 8: connect/header times available, response_time not found on rejected response, then all valid
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            return 404;
        }
    }
    proxy_next_upstream_tries 2;
    proxy_next_upstream http_404;
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_wasm hostcalls 'test=/t/log/properties on=upstream_info name=upstream.connect_time,upstream.header_time,upstream.response_time';
        proxy_pass http://test_upstream/;
    }
--- error_code: 404
--- grep_error_log eval: qr/upstream\.[_a-z]+: \w+ at UpstreamInfo|property not found: upstream\.[_a-z]+ at UpstreamInfo/
--- grep_error_log_out eval
qr/upstream\.connect_time: \d+ at UpstreamInfo
upstream\.header_time: \d+ at UpstreamInfo
property not found: upstream\.response_time at UpstreamInfo
upstream\.connect_time: \d+ at UpstreamInfo
upstream\.header_time: \d+ at UpstreamInfo
upstream\.response_time: \d+ at UpstreamInfo/
--- no_error_log
[emerg]
