# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

our $ExtResolver = $t::TestWasmX::extresolver;
our $ExtTimeout = $t::TestWasmX::exttimeout;

skip_no_ssl();

add_block_preprocessor(sub {
    my $block = shift;

    if (!defined $block->load_nginx_modules) {
        $block->set_value("load_nginx_modules", "ngx_http_echo_module");
    }
});

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: proxy_wasm - dispatch_https_call() sanity verify off, warn on
--- wasm_modules: hostcalls
--- config eval
qq{
    listen              $ENV{TEST_NGINX_SERVER_PORT2} ssl;
    server_name         hostname;
    ssl_certificate     $ENV{TEST_NGINX_DATA_DIR}/hostname_cert.pem;
    ssl_certificate_key $ENV{TEST_NGINX_DATA_DIR}/hostname_key.pem;

    resolver            1.1.1.1 ipv6=off;
    resolver_add        127.0.0.1 hostname;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=hostname:$ENV{TEST_NGINX_SERVER_PORT2} \
                              https=yes \
                              path=/dispatch \
                              on_http_call_response=echo_response_body';
        echo fail;
    }

    location /dispatch {
        echo ok;
    }
}
--- response_body
ok
--- error_log eval
[
    qr/\[warn\] .*? tls certificate not verified/,
    qr/\[warn\] .*? tls certificate host not verified/
]



=== TEST 2: proxy_wasm - dispatch_https_call() sanity verify off, warn off
--- main_config eval
qq{
    wasm {
        module hostcalls   $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_no_verify_warn off;
    }
}
--- config eval
qq{
    listen              $ENV{TEST_NGINX_SERVER_PORT2} ssl;
    server_name         hostname;
    ssl_certificate     $ENV{TEST_NGINX_DATA_DIR}/hostname_cert.pem;
    ssl_certificate_key $ENV{TEST_NGINX_DATA_DIR}/hostname_key.pem;

    resolver            1.1.1.1 ipv6=off;
    resolver_add        127.0.0.1 hostname;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=hostname:$ENV{TEST_NGINX_SERVER_PORT2} \
                              https=yes \
                              path=/dispatch \
                              on_http_call_response=echo_response_body';
        echo fail;
    }

    location /dispatch {
        echo ok;
    }
}
--- response_body
ok
--- no_error_log eval
[
    qr/tls certificate not verified/,
    qr/\[error\]/
]



=== TEST 3: proxy_wasm - dispatch_https_call() sanity verify on, over TCP/IP
--- valgrind
--- skip_no_debug
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate $ENV{TEST_NGINX_DATA_DIR}/cert.pem;
        tls_verify_cert         on;
    }
}
--- config
    listen              $TEST_NGINX_SERVER_PORT2 ssl;
    ssl_certificate     $TEST_NGINX_DATA_DIR/cert.pem;
    ssl_certificate_key $TEST_NGINX_DATA_DIR/key.pem;

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
Host: hostname.*
Connection: close.*
Content-Length: 0.*
--- error_log eval
qr/verifying tls certificate for "hostname:\d+" \(sni: "hostname"\)/
--- no_error_log
[error]



=== TEST 4: proxy_wasm - dispatch_https_call() sanity verify on, over unix domain socket
--- valgrind
--- skip_no_debug
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate $ENV{TEST_NGINX_DATA_DIR}/cert.pem;
        tls_verify_cert         on;
    }
}
--- http_config eval
qq{
    server {
        listen              unix:$ENV{TEST_NGINX_UNIX_SOCKET} ssl;
        ssl_certificate     $ENV{TEST_NGINX_DATA_DIR}/cert.pem;
        ssl_certificate_key $ENV{TEST_NGINX_DATA_DIR}/key.pem;

        location /headers {
            echo \$echo_client_request_headers;
        }
    }
}
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=unix:$ENV{TEST_NGINX_UNIX_SOCKET} \
                              authority=localhost \
                              https=yes \
                              path=/headers \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
}
--- response_body_like
GET \/headers HTTP\/1\.1.*
Host: localhost\s*
Connection: close.*
Content-Length: 0.*
--- error_log eval
"verifying tls certificate for \"unix:$ENV{TEST_NGINX_UNIX_SOCKET}\" (sni: \"localhost\")"
--- no_error_log
[error]



=== TEST 5: proxy_wasm - dispatch_https_call() fail verify on (expired)
--- timeout eval: $::ExtTimeout
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate $ENV{TEST_NGINX_DATA_DIR}/badssl_cert.pem;
        tls_verify_cert         on;
    }
}
--- config eval
qq{
    resolver         $::ExtResolver ipv6=off;
    resolver_timeout $::ExtTimeout;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=expired.badssl.com \
                              https=yes';
        echo fail;
    }
}
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - tls certificate verify error: \(10:certificate has expired\)/
--- no_error_log
[crit]



=== TEST 6: proxy_wasm - dispatch_https_call() sanity check_host on
--- valgrind
--- skip_no_debug
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
--- error_log eval
qr/checking tls certificate CN for "hostname:\d+" \(sni: "hostname"\)/
--- no_error_log
[error]



=== TEST 7: proxy_wasm - dispatch_https_call() fail check_host on
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
--- error_log eval
qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - tls certificate CN does not match \"localhost\" sni/
--- no_error_log
[crit]



=== TEST 8: proxy_wasm - dispatch_https_call() untrusted root
--- timeout eval: $::ExtTimeout
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate $ENV{TEST_NGINX_DATA_DIR}/badssl_cert.pem;
        tls_verify_cert         on;
    }
}
--- config eval
qq{
    resolver         $::ExtResolver ipv6=off;
    resolver_timeout $::ExtTimeout;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=untrusted-root.badssl.com \
                              https=yes';
        echo fail;
    }
}
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/tls certificate verify error: \(19:self.signed certificate in certificate chain\)/
--- no_error_log
[crit]



=== TEST 9: proxy_wasm - dispatch_https_call() attempt https at plain-text host
--- wasm_modules: hostcalls
--- config
    location /t {
        resolver     1.1.1.1 ipv6=off;
        resolver_add 127.0.0.1 localhost;

        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=localhost:$TEST_NGINX_SERVER_PORT \
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
    qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - tls handshake failed/
]



=== TEST 10: proxy_wasm - dispatch_https_call() bad trusted CA path
--- wasm_modules: hostcalls
--- tls_trusted_certificate: /tmp/invalid_filename.txt
--- error_log eval
qr/\[emerg\] .*?no such file/i
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 11: proxy_wasm - dispatch_https_call() bad trusted CA format
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



=== TEST 12: proxy_wasm - dispatch_https_call() no trusted CA
--- timeout eval: $::ExtTimeout
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_verify_cert  on;
    }
}
--- config eval
qq{
    resolver         $::ExtResolver ipv6=off;
    resolver_timeout $::ExtTimeout;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org \
                              https=yes';
        echo fail;
    }
}
--- error_code: 500
--- error_log eval
qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - tls certificate verify error: \((18|20):(self-signed certificate|unable to get local issuer certificate)\)/
--- no_error_log
[crit]
[emerg]



=== TEST 13: proxy_wasm - dispatch_https_call() empty trusted CA path
--- timeout eval: $::ExtTimeout
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate '';
        tls_verify_cert         on;
    }
}
--- config eval
qq{
    resolver         $::ExtResolver ipv6=off;
    resolver_timeout $::ExtTimeout;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=httpbin.org \
                              https=yes \
                              path=/headers';
        echo fail;
    }
}
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/(\[error\]|Uncaught RuntimeError|\s+).*?dispatch failed: tcp socket - tls certificate verify error: \((18|20):(self-signed certificate|unable to get local issuer certificate)\)/
--- no_error_log
[crit]



=== TEST 14: proxy_wasm - dispatch_https_call() SNI derived from host over TCP/IP
--- skip_no_debug
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
                              path=/dispatch \
                              on_http_call_response=echo_response_body';
        echo fail;
    }

    location /dispatch {
        echo ok;
    }
--- response_body
ok
--- error_log eval
[
    qr/upstream tls server name: "hostname"/,
    qr/checking tls certificate CN for "hostname:\d+" \(sni: "hostname"\)/,
]



=== TEST 15: proxy_wasm - dispatch_https_call() fail SNI derived from host with literal IPv4
SNI cannot be an IP address.
--- skip_no_debug
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
    }
}
--- config
    listen              $TEST_NGINX_SERVER_PORT2 ssl;
    ssl_certificate     $TEST_NGINX_DATA_DIR/hostname_cert.pem;
    ssl_certificate_key $TEST_NGINX_DATA_DIR/hostname_key.pem;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT2 \
                              https=yes \
                              path=/dispatch \
                              on_http_call_response=echo_response_body';
        echo fail;
    }

    location /dispatch {
        echo ok;
    }
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/could not derive tls sni from host \("127\.0\.0\.1:\d+"\)/
--- no_error_log
[crit]



=== TEST 16: proxy_wasm - dispatch_https_call() fail SNI derived from host with literal IPv6
SNI cannot be an IP address.
--- skip_no_debug
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
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
qr/could not derive tls sni from host \("\[0:0:0:0:0:0:0:1\]:\d+"\)/
--- no_error_log
[crit]



=== TEST 17: proxy_wasm - dispatch_https_call() SNI override with :authority over TCP/IP (debug)
--- skip_no_debug
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
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT2 \
                              authority=hostname \
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
--- error_log eval
[
    "upstream tls server name: \"hostname\"",
    qr/checking tls certificate CN for "127\.0\.0\.1:\d+" \(sni: "hostname"\)/,
]



=== TEST 18: proxy_wasm - dispatch_https_call() SNI override with :authority over TCP/IP (non-debug)
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
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT2 \
                              authority=hostname \
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
[crit]



=== TEST 19: proxy_wasm - dispatch_https_call() SNI override with :authority over unix domain socket (debug)
Unix sockets need to set :authority as SNI
--- skip_no_debug
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate $ENV{TEST_NGINX_DATA_DIR}/cert.pem;
        tls_verify_cert         on;
    }
}
--- http_config eval
qq{
    server {
        listen              unix:$ENV{TEST_NGINX_UNIX_SOCKET} ssl;
        ssl_certificate     $ENV{TEST_NGINX_DATA_DIR}/cert.pem;
        ssl_certificate_key $ENV{TEST_NGINX_DATA_DIR}/key.pem;

        location / {
            echo ok;
        }
    }
}
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=unix:$ENV{TEST_NGINX_UNIX_SOCKET} \
                              authority=hostname \
                              https=yes \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
}
--- response_body
ok
--- error_log eval
[
    "upstream tls server name: \"hostname\"",
    "verifying tls certificate for \"unix:$ENV{TEST_NGINX_UNIX_SOCKET}\" (sni: \"hostname\")"
]



=== TEST 20: proxy_wasm - dispatch_https_call() SNI override with :authority over unix domain socket (non-debug)
Unix sockets need to set :authority as SNI
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate $ENV{TEST_NGINX_DATA_DIR}/cert.pem;
        tls_verify_cert         on;
    }
}
--- http_config eval
qq{
    server {
        listen              unix:$ENV{TEST_NGINX_UNIX_SOCKET} ssl;
        ssl_certificate     $ENV{TEST_NGINX_DATA_DIR}/cert.pem;
        ssl_certificate_key $ENV{TEST_NGINX_DATA_DIR}/key.pem;

        location / {
            echo ok;
        }
    }
}
--- config eval
qq{
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=unix:$ENV{TEST_NGINX_UNIX_SOCKET} \
                              authority=hostname \
                              https=yes \
                              on_http_call_response=echo_response_body';
        echo fail;
    }
}
--- response_body
ok
--- no_error_log
[error]
[crit]



=== TEST 21: proxy_wasm - dispatch_https_call() sanity on_tick, verify off, warn on
--- valgrind
--- load_nginx_modules: ngx_http_echo_module
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

    resolver            1.1.1.1 ipv6=off;
    resolver_add        127.0.0.1 hostname;

    location /t {
        proxy_wasm hostcalls 'tick_period=100 \
                              on_tick=dispatch \
                              host=hostname:$TEST_NGINX_SERVER_PORT2 \
                              https=yes \
                              path=/dispatch';
        echo ok;
    }

    location /dispatch {
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/\[warn\] .*? tls certificate not verified/,
    qr/\[warn\] .*? tls certificate host not verified/,
    "on_root_http_call_response (id: 0, status: 200, headers: 5, body_bytes: 3, trailers: 0)",
]



=== TEST 22: proxy_wasm - dispatch_https_call() sanity on_tick, verify on, check_host on
--- skip_no_debug
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module hostcalls        $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
        tls_trusted_certificate $ENV{TEST_NGINX_DATA_DIR}/hostname_cert.pem;
        tls_verify_cert         on;
        tls_verify_host         on;
    }
}
--- config
    listen              $TEST_NGINX_SERVER_PORT2 ssl;
    server_name         hostname;
    ssl_certificate     $TEST_NGINX_DATA_DIR/hostname_cert.pem;
    ssl_certificate_key $TEST_NGINX_DATA_DIR/hostname_key.pem;

    resolver            1.1.1.1 ipv6=off;
    resolver_add        127.0.0.1 hostname;

    location /t {
        proxy_wasm hostcalls 'tick_period=100 \
                              on_tick=dispatch \
                              host=hostname:$TEST_NGINX_SERVER_PORT2 \
                              https=yes \
                              path=/dispatch';
        echo fail;
    }

    location /dispatch {
        echo ok;
    }
--- ignore_response_body
--- error_log eval
[
    qr/verifying tls certificate for "hostname:\d+" \(sni: "hostname"\)/,
    qr/checking tls certificate CN for "hostname:\d+" \(sni: "hostname"\)/,
    "on_root_http_call_response (id: 0, status: 200, headers: 5, body_bytes: 3, trailers: 0)",
]
