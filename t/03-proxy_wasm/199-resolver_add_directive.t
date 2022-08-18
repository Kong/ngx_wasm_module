# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind('wasmtime');

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: resolver_add directive - IPv4 sanity
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    server_name  hostname;
    resolver     1.1.1.1 ipv6=off;
    resolver_add 127.0.0.1 hostname;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=hostname:$TEST_NGINX_SERVER_PORT \
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



=== TEST 2: resolver_add directive - IPv6 sanity
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    listen       [::]:$TEST_NGINX_SERVER_PORT;
    server_name  hostname;
    resolver     1.1.1.1 ipv6=on;
    resolver_add 0:0:0:0:0:0:0:1 hostname;

    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call \
                              host=[0:0:0:0:0:0:0:1]:$TEST_NGINX_SERVER_PORT \
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



=== TEST 3: resolver_add directive - empty address
--- config
    resolver     1.1.1.1 ipv6=off;
    resolver_add '' hostname;
--- error_log eval
qr/\[emerg\] .*? invalid address value ""/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 4: resolver_add directive - empty host
--- config
    resolver     1.1.1.1 ipv6=off;
    resolver_add 127.0.0.1 '';
--- error_log eval
qr/\[emerg\] .*? invalid host value ""/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 5: resolver_add directive - bad IPv4 address
--- config
    resolver     1.1.1.1 ipv6=off;
    resolver_add 127.0.0 hostname;
--- error_log eval
qr/\[emerg\] .*? invalid address value "127.0.0"/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 6: resolver_add directive - bad IPv6 address
--- config
    resolver     1.1.1.1 ipv6=off;
    resolver_add 0:0:0:0:0:0 hostname;
--- error_log eval
qr/\[emerg\] .*? invalid address value "0:0:0:0:0:0"/
--- no_error_log
[error]
[crit]
--- must_die
