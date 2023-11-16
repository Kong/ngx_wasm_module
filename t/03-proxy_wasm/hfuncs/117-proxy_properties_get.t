# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

our $request_properties = join(',', qw(
    request.path
    request.url_path
    request.host
    request.scheme
    request.method
    request.referer
    request.useragent
    request.time
    request.id
    request.protocol
    request.query
    request.duration
    request.size
    request.total_size
));

plan_tests(5);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_property() - request properties on: request_headers
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
qq{
    location /t-especial {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/properties \
                              name=$::request_properties';
        echo ok;
    }
}
--- more_headers
x-request-id: id123
user-agent: a-user-agent
referer: a-referer
--- request
POST /t-especial?special_arg=true&special_arg2=false
hello
--- response_body
ok
--- grep_error_log eval: qr/request\.[_a-z]+: .+ at RequestHeaders/
--- grep_error_log_out eval
qr/request\.path: \/t-especial\?special_arg\=true\&special_arg2\=false at RequestHeaders
request\.url_path: \/t-especial at RequestHeaders
request\.host: [\w\.\-]+ at RequestHeaders
request\.scheme: http at RequestHeaders
request\.method: POST at RequestHeaders
request\.referer: a-referer at RequestHeaders
request\.useragent: a-user-agent at RequestHeaders
request\.time: \d+\.\d+ at RequestHeaders
request\.id: id123 at RequestHeaders
request\.protocol: HTTP\/1\.1 at RequestHeaders
request\.query: special_arg\=true\&special_arg2\=false at RequestHeaders
request\.duration: \d+\.\d+ at RequestHeaders
request\.size: 5 at RequestHeaders
request\.total_size: 187 at RequestHeaders/
--- no_error_log
[error]
[crit]



=== TEST 2: proxy_wasm - get_property() - request properties on: request_headers, request_body, response_headers, response_body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
my $phases = CORE::join(',', qw(
    request_headers
    request_body
    response_headers
    response_body
));

qq{
    location /t-especial {
        proxy_wasm hostcalls 'on=$phases \
                              test=/t/log/properties \
                              name=$::request_properties';
        echo ok;
    }
}
--- more_headers
x-request-id: id123
user-agent: a-user-agent
referer: a-referer
--- request
POST /t-especial?special_arg=true&special_arg2=false
hello
--- response_body
ok
--- grep_error_log eval: qr/request\.\w+: .+ at \w+/
--- grep_error_log_out eval
my $checks;
my @phases = qw(
    RequestHeaders
    RequestBody
    ResponseHeaders
    ResponseBody
    ResponseBody
);

foreach my $phase (@phases) {
    foreach my $var (split(',', $::request_properties)) {
        $checks .= "$var: .+ at $phase\n";
    }
}

qr/$checks/
--- no_error_log
[error]
[crit]



=== TEST 3: proxy_wasm - get_property() - request properties on: http_call_response
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /dispatched {
        echo ok;
    }

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=log_request_properties';
        echo failed;
    }
--- request
GET /t?hello=world
--- response_body
ok
--- grep_error_log eval: qr/request\.\w+: [\w\.\?\=\/\-]+ at \w+/
--- grep_error_log_out eval
qr/request.path: \/t\?hello=world at OnHttpCallResponse
request.url_path: \/t at OnHttpCallResponse
request.host: [\w\.\-]+ at OnHttpCallResponse
request.scheme: http at OnHttpCallResponse
request.method: GET at OnHttpCallResponse
request.time: \d+\.\d+ at OnHttpCallResponse
request.protocol: HTTP\/1\.1 at OnHttpCallResponse
request.query: hello\=world at OnHttpCallResponse
request.total_size: [1-9]+[0-9]+ at OnHttpCallResponse/
--- no_error_log
[error]
[crit]



=== TEST 4: proxy_wasm - get_property() - request.headers.[header_name] on: request_headers, request_body, response_headers, response_body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
my $phases = CORE::join(',', qw(
    request_headers
    request_body
    response_headers
    response_body
));

qq {
    location /t {
        proxy_wasm hostcalls 'on=$phases test=/t/log/properties \
                              name=request.headers.foo,request.headers.bar';
        echo ok;
    }
}
--- more_headers
foo: foo123
bar: bar123
--- request
POST /t
hello
--- response_body
ok
--- grep_error_log eval: qr/request\.headers\.\w+: \w+ at \w+/
--- grep_error_log_out eval
my $checks;
my @phases = qw(
    RequestHeaders
    RequestBody
    ResponseHeaders
    ResponseBody
    ResponseBody
);

foreach my $phase (@phases) {
    $checks .= "request.headers.foo: foo123 at $phase\n";
    $checks .= "request.headers.bar: bar123 at $phase\n";
}

qr/$checks/
--- no_error_log
[error]
[crit]



=== TEST 5: proxy_wasm - get_property() - response properties on: response_headers, response_body
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers,response_body \
                              test=/t/log/properties \
                              name=response.code,response.size,response.total_size';
        echo ok;
    }
--- response_body
ok
--- grep_error_log eval: qr/response\.\w+: \w+ at \w+/
--- grep_error_log_out eval
qr/response\.code: 200 at ResponseHeaders
response\.size: 0 at ResponseHeaders
response\.total_size: 0 at ResponseHeaders
response\.code: 200 at ResponseBody
response\.size: 0 at ResponseBody
response\.total_size: 0 at ResponseBody
response\.code: 200 at ResponseBody
response\.size: 0 at ResponseBody
response\.total_size: 0 at ResponseBody/
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm - get_property() - response.headers.header_name on: response_headers, response_body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config eval
qq{
    upstream test_upstream {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location / {
            add_header foo foo123;
            add_header bar bar123;
            echo ok;
        }
    }
}
--- config
    location /t {
        proxy_wasm hostcalls 'on=response_headers,response_body \
                              test=/t/log/properties \
                              name=response.headers.foo,response.headers.bar';
        proxy_pass http://test_upstream/;
    }
--- response_body
ok
--- grep_error_log eval: qr/response\.headers\.\w+: \w+ at \w+/
--- grep_error_log_out eval
my $checks;
my @phases = qw(
    ResponseHeaders
    ResponseBody
    ResponseBody
);

foreach my $phase (@phases) {
    $checks .= "response.headers.foo: foo123 at $phase\n";
    $checks .= "response.headers.bar: bar123 at $phase\n";
}

qr/$checks/
--- no_error_log
[error]
[crit]



=== TEST 7: proxy_wasm - get_property() - upstream properties on: response_headers, response_body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config eval
qq{
    upstream test_upstream {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location / {
            echo ok;
        }
    }
}
--- config
    location /t {
        proxy_pass http://test_upstream/;
        proxy_wasm hostcalls 'on=response_headers,response_body \
                              test=/t/log/properties \
                              name=upstream.address,upstream.port';
    }
--- response_body
ok
--- grep_error_log eval: qr/upstream\.\w+: [\w\.\/]+ at \w+/
--- grep_error_log_out eval
my $checks;
my @phases = qw(
    ResponseHeaders
    ResponseBody
    ResponseBody
);

foreach my $phase (@phases) {
    $checks .= "upstream.address: unix at $phase\n";
    $checks .= "upstream.port: [\\w\.\/]+ at $phase\n";
}

qr/$checks/
--- no_error_log
[error]
[crit]



=== TEST 8: proxy_wasm - get_property() - upstream properties (IPv6) on: response_headers

Disabled on GitHub Actions due to IPv6 constraint.

--- skip_eval: 5: system("ping6 -c 1 ipv6.google.com >/dev/null 2>&1") ne 0 || defined $ENV{GITHUB_ACTIONS}
--- timeout eval: $::ExtTimeout
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_pass https://ipv6.google.com/;
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/properties \
                              name=upstream.address,upstream.port';
    }
--- response_body_like
\A\<!doctype html\>.*
--- grep_error_log eval: qr/upstream\.\w+: (\w|\[|\]|:)+/
--- grep_error_log_out eval
qr/upstream\.address: \[[a-z0-9]+:[a-z0-9]+:[a-z0-9]+:[a-z0-9]+::[a-z0-9]+\]
upstream\.port: [0-9]+/
--- no_error_log
[error]
[crit]



=== TEST 9: proxy_wasm - get_property() - upstream properties access without an upstream
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        echo ok;
        proxy_wasm hostcalls 'on=response_headers \
                              test=/t/log/properties \
                              name=upstream.address,upstream.port';
    }
--- ignore_response_body
--- error_log
property not found: upstream.address
property not found: upstream.port
--- no_error_log
[error]
[crit]



=== TEST 10: proxy_wasm - get_property() - connection properties on: request_headers, response_headers, response_body
--- skip_eval: 5: $::nginxV !~ m/built with OpenSSL/
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
my $phases = CORE::join(',', qw(
    request_headers
    response_headers
    response_body
));

my $vars = CORE::join(',', qw(
    source.address
    source.port
    connection.tls_version
    connection.requested_server_name
    connection.mtls
    connection.id
));

qq {
    listen              $ENV{TEST_NGINX_SERVER_PORT2} ssl;
    server_name         hostname;
    ssl_protocols       TLSv1.2;
    ssl_certificate     $ENV{TEST_NGINX_DATA_DIR}/hostname_cert.pem;
    ssl_certificate_key $ENV{TEST_NGINX_DATA_DIR}/hostname_key.pem;

    resolver 1.1.1.1 ipv6=off;
    resolver_add 127.0.0.1 hostname;

    location /dispatch {
        proxy_wasm hostcalls 'on=$phases \
                              test=/t/log/properties \
                              name=$vars';
        echo ok;
    }

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=hostname:$ENV{TEST_NGINX_SERVER_PORT2} \
                              https=yes \
                              path=/dispatch \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
}
--- request
POST /t
hello
--- response_body
ok
--- grep_error_log eval: qr/\w+\.\w+: [\w\.]+ at \w+/
--- grep_error_log_out eval
my $checks;
my @phases = qw(
    RequestHeaders
    ResponseHeaders
    ResponseBody
    ResponseBody
);

foreach my $phase (@phases) {
    $checks .= "source.address: [\\w\.]+ at $phase\n";
    $checks .= "source.port: \\d+ at $phase\n";
    $checks .= "connection.tls_version: TLSv1.2 at $phase\n";
    $checks .= "connection.requested_server_name: hostname at $phase\n";
    $checks .= "connection.mtls: false at $phase\n";
    $checks .= "connection.id: \\d+ at $phase\n";
}

qr/$checks/
--- no_error_log
[error]
[crit]



=== TEST 11: proxy_wasm - get_property() - connection properties (mTLS) on: request_headers, response_headers, response_body

TODO: fix what looks like an nginx memleak:

==15082== 8,002 (928 direct, 7,074 indirect) bytes in 1 blocks are definitely lost in loss record 92 of 93
==15082==    at 0x4848899: malloc (in /usr/libexec/valgrind/vgpreload_memcheck-amd64-linux.so)
==15082==    by 0x58B041D: CRYPTO_zalloc (in /usr/lib/x86_64-linux-gnu/libcrypto.so.3)
==15082==    by 0x498488A: SSL_SESSION_new (in /usr/lib/x86_64-linux-gnu/libssl.so.3)
==15082==    by 0x4984D35: ??? (in /usr/lib/x86_64-linux-gnu/libssl.so.3)
==15082==    by 0x49A6F79: ??? (in /usr/lib/x86_64-linux-gnu/libssl.so.3)
==15082==    by 0x49A67FC: ??? (in /usr/lib/x86_64-linux-gnu/libssl.so.3)
==15082==    by 0x169241: ngx_ssl_handshake.part.0 (ngx_event_openssl.c:1781)
==15082==    by 0x1972A4: ngx_http_upstream_ssl_init_connection.part.0 (ngx_http_upstream.c:1727)
==15082==    by 0x189E61: ngx_http_read_client_request_body (ngx_http_request_body.c:84)
==15082==    by 0x189E61: ngx_http_read_client_request_body (ngx_http_request_body.c:32)
==15082==    by 0x1CF16E: ngx_http_proxy_handler (ngx_http_proxy_module.c:1023)
==15082==    by 0x1CF16E: ngx_http_proxy_handler (ngx_http_proxy_module.c:935)
==15082==    by 0x17A603: ngx_http_core_content_phase (ngx_http_core_module.c:1261)
==15082==    by 0x174B3C: ngx_http_core_run_phases (ngx_http_core_module.c:875)
==15082==    by 0x1816A7: ngx_http_process_request_headers (ngx_http_request.c:1496)
==15082==    by 0x181B35: ngx_http_process_request_line (ngx_http_request.c:1163)
==15082==    by 0x16405A: ngx_epoll_process_events (ngx_epoll_module.c:901)
==15082==    by 0x158A84: ngx_process_events_and_timers (ngx_event.c:248)
==15082==    by 0x163074: ngx_single_process_cycle (ngx_process_cycle.c:300)
==15082==    by 0x135378: main (nginx.c:380)

Also caused without ngx_wasm_module compiled in, and with tcp & unix upstreams.

--- skip_eval: 5: ($::nginxV !~ m/built with OpenSSL/ || $ENV{TEST_NGINX_USE_VALGRIND} == 1)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config eval
my $phases = CORE::join(',', qw(
    request_headers
    response_headers
    response_body
));

qq {
    upstream hostname {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen                   unix:$ENV{TEST_NGINX_UNIX_SOCKET} ssl;
        server_name              hostname;
        ssl_protocols            TLSv1.2;
        ssl_certificate          $ENV{TEST_NGINX_DATA_DIR}/hostname_cert.pem;
        ssl_certificate_key      $ENV{TEST_NGINX_DATA_DIR}/hostname_key.pem;
        ssl_client_certificate   $ENV{TEST_NGINX_DATA_DIR}/hostname_cert.pem;
        ssl_verify_client        on;

        location / {
            proxy_wasm hostcalls 'on=$phases \
                                  test=/t/log/properties \
                                  name=connection.mtls,connection.tls_version';
            echo ok;
        }
    }
}
--- config
    location /t {
        proxy_pass                     https://hostname;
        proxy_ssl_certificate          $TEST_NGINX_DATA_DIR/hostname_cert.pem;
        proxy_ssl_certificate_key      $TEST_NGINX_DATA_DIR/hostname_key.pem;
        proxy_ssl_trusted_certificate  $TEST_NGINX_DATA_DIR/hostname_cert.pem;
        proxy_ssl_verify               on;
    }
--- response_body
ok
--- grep_error_log eval: qr/[\.\w]+: [\w\.]+ at \w+/
--- grep_error_log_out eval
my $checks;
my @phases = qw(
    RequestHeaders
    ResponseHeaders
    ResponseBody
    ResponseBody
);

foreach my $phase (@phases) {
    $checks .= "connection.mtls: true at $phase\n";
    $checks .= "connection.tls_version: TLSv1\.2 at $phase\n";
}

qr/$checks/
--- no_error_log
[error]
[crit]



=== TEST 12: proxy_wasm - get_property() - proxy-wasm properties on: request_headers, request_body, response_headers, response_body
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
my $phases = CORE::join(',', qw(
    request_headers
    request_body
    response_headers
    response_body
));

qq {
    location /t {
        proxy_wasm hostcalls 'on=$phases \
                              test=/t/log/properties \
                              name=plugin_name,plugin_root_id';
        echo ok;
    }
}
--- request
POST /t
hello
--- response_body
ok
--- grep_error_log eval: qr/plugin\w+: \w+ at \w+/
--- grep_error_log_out eval
my $checks;
my @phases = qw(
    RequestHeaders
    RequestBody
    ResponseHeaders
    ResponseBody
    ResponseBody
);

foreach my $phase (@phases) {
    $checks .= "plugin_name: \\w+ at $phase\n";
    $checks .= "plugin_root_id: \\d+ at $phase\n";
}

qr/$checks/
--- no_error_log
[error]
[crit]



=== TEST 13: proxy_wasm - get_property() - uri encoded request.path on: request_headers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/properties \
                              name=request.path';
        echo ok;
    }
--- request eval
use URI::Escape;
"GET /t?foo=" . uri_escape("std::min") . "&bar=" . uri_escape("[1,2]")
--- response_body
ok
--- grep_error_log eval: qr/\w+\.\w+\: .+/
--- grep_error_log_out eval
qr/request.path: \/t\?foo=std\:\:min\&bar=\[1,2\]/
--- no_error_log
[error]
[crit]



=== TEST 14: proxy_wasm - get_property() - explicitly unsupported properties on: request_headers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config eval
my $vars = CORE::join(',', qw(
    response.code_details
    response.flags
    connection.termination_details
    node
    cluster_name
    cluster_metadata
    listener_direction
    listener_metadata
    route_name
    route_metadata
    upstream_host_metadata
));

qq{
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/properties \
                              name=$vars';
        echo ok;
    }
};
--- response_body
ok
--- grep_error_log eval: qr/"(\w|\.)+" property not supported/
--- grep_error_log_out eval
qr/"response.code_details" property not supported
"response.flags" property not supported
"connection.termination_details" property not supported
"node" property not supported
"cluster_name" property not supported
"cluster_metadata" property not supported
"listener_direction" property not supported
"listener_metadata" property not supported
"route_name" property not supported
"route_metadata" property not supported
"upstream_host_metadata" property not supported/
--- no_error_log
[crit]
[emerg]



=== TEST 15: proxy_wasm - get_property() - unknown property on: request_headers
--- wasm_modules: hostcalls
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/log/property \
                              name=nonexistent_property';
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? property not found: nonexistent_property,/
--- no_error_log
[error]
[crit]
