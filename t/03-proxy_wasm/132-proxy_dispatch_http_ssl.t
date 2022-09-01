# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_no_ssl();
skip_valgrind('wasmtime');

plan tests => repeat_each() * (blocks() * 4);

add_block_preprocessor(sub {
    my $block = shift;
    if (!defined $block->load_nginx_modules) {
        $block->set_value("load_nginx_modules", "ngx_http_echo_module");
    }
});

run_tests();

__DATA__

=== TEST 1: proxy_wasm - dispatch_https_call() verify off, warn, default port, ok
--- wasm_modules: hostcalls
--- config
    resolver 1.1.1.1 ipv6=off;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org \
                              https=yes \
                              path=/headers \
                              headers=X-Thing:foo|X-Thing:bar|Hello:world \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
--- response_body_like
\s*"Hello": "world",\s*
.*?
\s*"X-Thing": "foo,bar"\s*
--- error_log eval
[
    qr/\[warn\] .*? tls certificate not verified/,
    qr/\[warn\] .*? tls certificate host not verified/
]



=== TEST 2: proxy_wasm - dispatch_https_call() verify off, no warn, default port, ok
--- main_config eval
qq{
    wasm {
        module hostcalls   $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_no_verify_warn off;
    }
}
--- config
    resolver 1.1.1.1 ipv6=off;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org \
                              https=yes \
                              headers=X-Thing:foo|X-Thing:bar|Hello:world \
                              path=/headers \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
--- response_body_like
\s*"Hello": "world",\s*
.*?
\s*"X-Thing": "foo,bar"\s*
--- no_error_log eval
[
    qr/tls certificate not verified/,
    qr/\[error\]/
]



=== TEST 3: proxy_wasm - dispatch_https_call() verify on, ok
--- skip_no_debug: 4
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate $ENV{TEST_NGINX_DATA_DIR}/cert.pem;
        tls_verify_cert         on;
    }
}
--- config
    # TODO: unix support in tcp_sock
    #listen              unix:$ENV{TEST_NGINX_UNIX_SOCKET} ssl;

    listen              $TEST_NGINX_SERVER_PORT2 ssl;
    ssl_certificate     $TEST_NGINX_DATA_DIR/cert.pem;
    ssl_certificate_key $TEST_NGINX_DATA_DIR/key.pem;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT2 \
                              https=yes \
                              path=/headers \
                              on_http_call_response=echo_response_body';
        echo fail;
    }

    location /headers {
        echo $echo_client_request_headers;
    }
--- response_body_like
GET \/headers HTTP\/1\.1.*
Host: 127\.0\.0\.1:\d+.*
Connection: close.*
Content-Length: 0.*
--- error_log
verifying tls certificate for "127.0.0.1"
--- no_error_log
[error]



=== TEST 4: proxy_wasm - dispatch_https_call() verify on, fail (expired)
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate $ENV{TEST_NGINX_DATA_DIR}/badssl_cert.pem;
        tls_verify_cert         on;
    }
}
--- config
    resolver 1.1.1.1 ipv6=off;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=expired.badssl.com \
                              https=yes';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
dispatch failed (tls certificate verify error: (10:certificate has expired))
--- no_error_log
[crit]



=== TEST 5: proxy_wasm - dispatch_https_call() check_host on, ok
--- skip_no_debug: 4
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate $ENV{TEST_NGINX_DATA_DIR}/hostname_cert.pem;
        tls_verify_host         on;
    }
}
--- config
    listen              $TEST_NGINX_SERVER_PORT2 ssl;
    server_name         hostname;
    ssl_certificate     $TEST_NGINX_DATA_DIR/hostname_cert.pem;
    ssl_certificate_key $TEST_NGINX_DATA_DIR/hostname_key.pem;

    resolver 1.1.1.1 ipv6=off;
    resolver_add 127.0.0.1 hostname;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=hostname:$TEST_NGINX_SERVER_PORT2 \
                              https=yes \
                              path=/headers \
                              on_http_call_response=echo_response_body';
        echo fail;
    }

    location /headers {
        echo $echo_client_request_headers;
    }
--- response_body_like
GET \/headers HTTP\/1\.1.*
Host: hostname:\d+.*
Connection: close.*
Content-Length: 0.*
--- error_log
checking tls certificate host for "hostname"
--- no_error_log
[error]



=== TEST 6: proxy_wasm - dispatch_https_call() check_host on, fail
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate $ENV{TEST_NGINX_DATA_DIR}/hostname_cert.pem;
        tls_verify_host         on;
    }
}
--- config
    listen              $TEST_NGINX_SERVER_PORT2 ssl;
    server_name         localhost;
    ssl_certificate     $TEST_NGINX_DATA_DIR/hostname_cert.pem;
    ssl_certificate_key $TEST_NGINX_DATA_DIR/hostname_key.pem;

    resolver 1.1.1.1 ipv6=off;
    resolver_add 127.0.0.1 localhost;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=localhost:$TEST_NGINX_SERVER_PORT2 \
                              https=yes';
        echo fail;
    }

    location /headers {
        echo $echo_client_request_headers;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
dispatch failed (tls certificate does not match "localhost")
--- no_error_log
[crit]



=== TEST 7: proxy_wasm - dispatch_https_call() untrusted root
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate $ENV{TEST_NGINX_DATA_DIR}/badssl_cert.pem;
        tls_verify_cert         on;
    }
}
--- config
    resolver 1.1.1.1 ipv6=off;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=untrusted-root.badssl.com \
                              https=yes';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/tls certificate verify error: \(19:self.signed certificate in certificate chain\)\)/
--- no_error_log
[crit]



=== TEST 8: proxy_wasm - dispatch_https_call() attempt https at plain-text host
--- timeout_no_valgrind: 1
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              https=yes \
                              path=/dispatched';
        echo fail;
    }

    location /dispatched {
        echo -n fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
[
    qr/\[crit\] .*? SSL_do_handshake\(\) failed/,
    qr/\[error\] .*? dispatch failed \(tls handshake failed\)/,
]



=== TEST 9: proxy_wasm - dispatch_https_call() bad trusted CA path
--- wasm_modules: hostcalls
--- tls_trusted_certificate: /tmp/invalid_filename.txt
--- error_log eval
qr/\[emerg\] .*?no such file/i
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 10: proxy_wasm - dispatch_https_call() bad trusted CA format
--- main_config
    wasm {
        tls_trusted_certificate $TEST_NGINX_HTML_DIR/ca.pem;
    }
--- error_log eval
qr/\[emerg\] .*?no certificate or crl found/i
--- no_error_log
[error]
[crit]
--- must_die
--- user_files
>>> ca.pem
foo



=== TEST 11: proxy_wasm - dispatch_https_call() no trusted CA
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_verify_cert  on;
    }
}
--- config
    resolver 1.1.1.1 ipv6=off;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org \
                              https=yes';
        echo fail;
    }
--- error_code: 500
--- error_log
dispatch failed (tls certificate verify error: (20:unable to get local issuer certificate))
--- no_error_log
[crit]
[emerg]



=== TEST 12: proxy_wasm - dispatch_https_call() empty trusted CA path
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate '';
        tls_verify_cert         on;
    }
}
--- config
    resolver 1.1.1.1 ipv6=off;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org \
                              https=yes \
                              path=/headers';
        echo fail;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
dispatch failed (tls certificate verify error: (20:unable to get local issuer certificate))
--- no_error_log
[crit]



=== TEST 13: proxy_wasm - dispatch_https_call() SNI not set with literal IPv6
--- skip_no_debug: 4
--- wasm_modules: hostcalls
--- config
    listen              [::]:$TEST_NGINX_SERVER_PORT ssl;
    ssl_certificate     $TEST_NGINX_DATA_DIR/cert.pem;
    ssl_certificate_key $TEST_NGINX_DATA_DIR/key.pem;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=[0:0:0:0:0:0:0:1]:$TEST_NGINX_SERVER_PORT \
                              https=yes \
                              path=/dispatch \
                              on_http_call_response=echo_response_body';
        echo fail;
    }

    location /dispatch {
        echo ok;
    }
--- response_body
ok
--- no_error_log
[error]
upstream tls server name



=== TEST 14: proxy_wasm - dispatch_https_call() SNI set with hostname
--- skip_no_debug: 4
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate $ENV{TEST_NGINX_DATA_DIR}/hostname_cert.pem;
    }
}
--- config
    listen              $TEST_NGINX_SERVER_PORT2 ssl;
    server_name         hostname;
    ssl_certificate     $TEST_NGINX_DATA_DIR/hostname_cert.pem;
    ssl_certificate_key $TEST_NGINX_DATA_DIR/hostname_key.pem;

    resolver 1.1.1.1 ipv6=off;
    resolver_add 127.0.0.1 hostname;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=hostname:$TEST_NGINX_SERVER_PORT2 \
                              https=yes \
                              path=/dispatch \
                              on_http_call_response=echo_response_body';
        echo fail;
    }

    location /dispatch {
        echo ok;
    }
--- response_body
ok
--- error_log
upstream tls server name: "hostname"
--- no_error_log
[error]
