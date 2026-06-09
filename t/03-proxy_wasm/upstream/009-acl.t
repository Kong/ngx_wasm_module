# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan tests => 103;
run_tests();

__DATA__

=== TEST 1: parsing - valid ACL block loads cleanly
--- wasm_modules: hostcalls
--- wasm_acl
    acl allow_only {
        allow       10.0.0.0/8;
        allow       127.0.0.0/8;
        allow_host  example.com;
        deny        127.0.0.13;
    }
--- config
    location /t {
        return 200;
    }
--- request
GET /t
--- response_body_like: ^
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 2: parsing - duplicate ACL name is rejected at config load
--- wasm_modules: hostcalls
--- wasm_acl
    acl dup {
        allow 10.0.0.0/8;
    }
    acl dup {
        allow 192.168.0.0/16;
    }
--- config
    location /t {
        return 200;
    }
--- must_die
--- error_log eval
qr/\[emerg\].*?\[wasm\] duplicate acl "dup"/



=== TEST 3: parsing - invalid CIDR is rejected
--- wasm_modules: hostcalls
--- wasm_acl
    acl bad {
        allow not.an.address;
    }
--- config
    location /t {
        return 200;
    }
--- must_die
--- error_log eval
qr/\[emerg\].*?\[wasm\] invalid CIDR "not\.an\.address"/



=== TEST 4: parsing - unknown acl=name on proxy_wasm directive fails
--- wasm_modules: hostcalls
--- wasm_acl
    acl present {
        allow 127.0.0.0/8;
    }
--- config
    location /t {
        proxy_wasm hostcalls acl=missing;
        return 200;
    }
--- must_die
--- error_log eval
qr/\[emerg\].*?unknown acl "missing"/



=== TEST 5: parsing - IPv6 CIDR accepted
--- wasm_modules: hostcalls
--- wasm_acl
    acl ipv6_only {
        allow ::1;
        allow 2001:db8::/32;
        deny  2001:db8::13;
    }
--- config
    location /t {
        return 200;
    }
--- request
GET /t
--- response_body_like: ^
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 6: runtime - no acl= parameter, set_upstream accepts any IP
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config
    server {
        listen       8891;
        location / {
            echo "allowed";
        }
    }
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891';
        proxy_pass http://test_upstream/;
    }
--- response_body
allowed
--- error_log
[wasm] set upstream peer "127.0.0.1:8891"
--- no_error_log
forbidden upstream address
[emerg]



=== TEST 7: runtime - allow CIDR, set_upstream accepts in-range IP
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- wasm_acl
    acl loop_only {
        allow 127.0.0.0/8;
    }
--- http_config
    server {
        listen       8891;
        location / {
            echo "allowed";
        }
    }
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891' acl=loop_only;
        proxy_pass http://test_upstream/;
    }
--- response_body
allowed
--- error_log
[wasm] set upstream peer "127.0.0.1:8891"
--- no_error_log
forbidden upstream address
[emerg]



=== TEST 8: runtime - allow CIDR, set_upstream rejects out-of-range IP
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- wasm_acl
    acl private_only {
        allow 10.0.0.0/8;
    }
--- http_config
    server {
        listen       8891;
        location / {
            echo "fallback";
        }
    }
    upstream test_upstream {
        server 127.0.0.1:8891;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.1 port=8891' acl=private_only;
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log eval
qr/forbidden upstream address "127\.0\.0\.1"/
--- no_error_log
[emerg]



=== TEST 9: runtime - deny CIDR wins over allow CIDR
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- wasm_acl
    acl with_carveout {
        allow 127.0.0.0/8;
        deny  127.0.0.13;
    }
--- http_config
    server {
        listen       8891;
        location / {
            echo "fallback";
        }
    }
    upstream test_upstream {
        server 127.0.0.1:8891;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=127.0.0.13 port=8891' acl=with_carveout;
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log eval
qr/forbidden upstream address "127\.0\.0\.13"/
--- no_error_log
[emerg]



=== TEST 10: runtime - dispatch with hostname not in allow_host is rejected
--- wasm_modules: hostcalls
--- wasm_acl
    acl strict {
        allow_host only.allowed.host;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call host=other.forbidden.host:80' acl=strict;
    }
--- error_code: 500
--- error_log eval
qr/dispatch forbidden: "other\.forbidden\.host:80"/
--- no_error_log
[emerg]



=== TEST 11: runtime - dispatch with IP outside allow CIDR rejected
--- wasm_modules: hostcalls
--- wasm_acl
    acl loop_ip {
        allow 127.0.0.0/8;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call host=10.99.99.99:80' acl=loop_ip;
    }
--- error_code: 500
--- error_log eval
qr/dispatch forbidden: "10\.99\.99\.99:80"/
--- no_error_log
[emerg]



=== TEST 12: runtime - dispatch with IP in deny CIDR rejected even inside allow range
--- wasm_modules: hostcalls
--- wasm_acl
    acl pin_dangerous {
        allow 127.0.0.0/8;
        deny  127.0.0.13;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call host=127.0.0.13:80' acl=pin_dangerous;
    }
--- error_code: 500
--- error_log eval
qr/dispatch forbidden: "127\.0\.0\.13:80"/
--- no_error_log
[emerg]



=== TEST 13: runtime - resolve with unknown host rejected by check_host
--- wasm_modules: hostcalls
--- wasm_acl
    acl names {
        allow_host allowed.example;
    }
--- config
    resolver 127.0.0.1:1953 ipv6=off;
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve name=forbidden.example' acl=names;
        return 200;
    }
--- error_code: 500
--- error_log eval
qr/resolve forbidden: "forbidden\.example"/
--- no_error_log
[emerg]



=== TEST 14: runtime - IPv6 deny via set_upstream
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- wasm_acl
    acl v6 {
        allow ::1;
        deny  2001:db8::13;
    }
--- http_config
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_upstream on=upstream_select ip=[2001:db8::13] port=8891' acl=v6;
        proxy_pass http://test_upstream/;
    }
--- error_code: 500
--- error_log eval
qr/forbidden upstream address "\[2001:db8::13\]"/
--- no_error_log
[emerg]



=== TEST 15: runtime - dispatch with hostname in allow_host accepted
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- wasm_acl
    acl rules {
        allow_host httpbin.org;
        allow      127.0.0.0/8;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/dispatch_http_call \
                              host=httpbin.org' acl=rules;
        echo ok;
    }
--- no_error_log
dispatch forbidden
[emerg]



=== TEST 16: runtime - dispatch with IP in allow CIDR accepted
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- wasm_acl
    acl loop_ip {
        allow 127.0.0.0/8;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_body \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=echo_response_body' acl=loop_ip;
        echo failed;
    }

    location /dispatched {
        return 200 "ok";
    }
--- request
POST /t

Hello world
--- response_body
ok
--- no_error_log
dispatch forbidden
[emerg]



=== TEST 17: runtime - dispatch with no acl= parameter accepted
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls 'on=request_body \
                              test=/t/dispatch_http_call \
                              host=127.0.0.1:$TEST_NGINX_SERVER_PORT \
                              path=/dispatched \
                              on_http_call_response=echo_response_body';
        echo failed;
    }

    location /dispatched {
        return 200 "ok";
    }
--- request
POST /t

Hello world
--- response_body
ok
--- no_error_log
dispatch forbidden
[emerg]



=== TEST 18: runtime - resolve allowed hostname succeeds
--- timeout eval: $t::TestWasmX::exttimeout
--- wasm_modules: hostcalls
--- wasm_acl
    acl names {
        allow_host httpbin.org;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve name=httpbin.org' acl=names;
        return 200;
    }
--- error_log eval
qr/resolved \(yielding\) httpbin\.org to \[\d+(?:, \d+)+\]/
--- no_error_log
resolve forbidden
[emerg]



=== TEST 19: runtime - resolve records IP on root exec; later set_upstream accepts it
--- wasm_modules: upstream_resolve
--- wasm_acl
    acl stamp_only {
        allow_host httpbin.org;
    }
--- http_config
    upstream test_upstream {
        server 0.0.0.0;
        wasm_upstream_select;
    }
--- config
    resolver 1.1.1.1 ipv6=off;
    location /t {
        proxy_wasm upstream_resolve 'host=httpbin.org port=80' acl=stamp_only;
        proxy_pass http://test_upstream/;
    }
--- error_log eval
[
    qr/upstream_resolve: resolved httpbin\.org to \d+\.\d+\.\d+\.\d+/,
    qr/upstream_resolve: set_upstream \d+\.\d+\.\d+\.\d+:80/,
    qr/\[wasm\] set upstream peer "\d+\.\d+\.\d+\.\d+:80"/,
]
--- no_error_log
forbidden upstream address
resolve forbidden
[emerg]



=== TEST 22: runtime - allow_host wildcard accepts subdomain
--- wasm_modules: hostcalls
--- wasm_acl
    acl wild {
        allow_host *.example.com;
    }
--- config
    resolver 127.0.0.1:1953 ipv6=off;
    resolver_timeout 100ms;
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve name=api.example.com' acl=wild;
        return 200;
    }
--- no_error_log
resolve forbidden
[emerg]



=== TEST 23: runtime - allow_host wildcard rejects bare apex domain
--- wasm_modules: hostcalls
--- wasm_acl
    acl wild {
        allow_host *.example.com;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve name=example.com' acl=wild;
        return 200;
    }
--- error_code: 500
--- error_log eval
qr/resolve forbidden: "example\.com"/
--- no_error_log
[emerg]



=== TEST 24: runtime - deny_host wins over wildcard allow_host
--- wasm_modules: hostcalls
--- wasm_acl
    acl carveout {
        allow_host *.example.com;
        deny_host  evil.example.com;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve name=evil.example.com' acl=carveout;
        return 200;
    }
--- error_code: 500
--- error_log eval
qr/resolve forbidden: "evil\.example\.com"/
--- no_error_log
[emerg]



=== TEST 25: runtime - deny_host alone (no allow) rejects the host
--- wasm_modules: hostcalls
--- wasm_acl
    acl deny_only {
        deny_host api.foo.com;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve name=api.foo.com' acl=deny_only;
        return 200;
    }
--- error_code: 500
--- error_log eval
qr/resolve forbidden: "api\.foo\.com"/
--- no_error_log
[emerg]



=== TEST 26: runtime - deny_host wildcard inside allow_host wildcard
--- wasm_modules: hostcalls
--- wasm_acl
    acl subtree {
        allow_host  *.example.com;
        deny_host   *.internal.example.com;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve name=db.internal.example.com' acl=subtree;
        return 200;
    }
--- error_code: 500
--- error_log eval
qr/resolve forbidden: "db\.internal\.example\.com"/
--- no_error_log
[emerg]



=== TEST 27: runtime - hostname match is case-insensitive
--- wasm_modules: hostcalls
--- wasm_acl
    acl ci {
        allow_host API.Example.COM;
    }
--- config
    resolver 127.0.0.1:1953 ipv6=off;
    resolver_timeout 100ms;
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve name=api.example.com' acl=ci;
        return 200;
    }
--- no_error_log
resolve forbidden
[emerg]



=== TEST 28: runtime - exact deny_host overrides wildcard allow_host
--- wasm_modules: hostcalls
--- wasm_acl
    acl exact_wins {
        allow_host *.example.com;
        deny_host  api.example.com;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve name=api.example.com' acl=exact_wins;
        return 200;
    }
--- error_code: 500
--- error_log eval
qr/resolve forbidden: "api\.example\.com"/
--- no_error_log
[emerg]



=== TEST 29: parsing - same host in allow and deny is rejected
--- wasm_modules: hostcalls
--- wasm_acl
    acl conflict {
        allow_host foo.com;
        deny_host  foo.com;
    }
--- config
    location /t {
        return 200;
    }
--- must_die
--- error_log eval
qr/\[emerg\].*duplicate host "foo\.com" in acl/



=== TEST 30: parsing - wildcard in middle of hostname is rejected
--- wasm_modules: hostcalls
--- wasm_acl
    acl bad_middle {
        allow_host api.*.internal;
    }
--- config
    location /t {
        return 200;
    }
--- must_die
--- error_log eval
qr/\[emerg\].*invalid wildcard host "api\.\*\.internal"/



=== TEST 31: runtime - tail wildcard matches across TLDs
--- wasm_modules: hostcalls
--- wasm_acl
    acl tail {
        allow_host example.*;
    }
--- config
    resolver 127.0.0.1:1953 ipv6=off;
    resolver_timeout 100ms;
    location /t {
        proxy_wasm hostcalls 'test=/t/resolve name=example.test' acl=tail;
        return 200;
    }
--- no_error_log
resolve forbidden
[emerg]



=== TEST 32: runtime - host:port strips port before ACL lookup
--- wasm_modules: hostcalls
--- wasm_acl
    acl hp {
        allow_host api.example.com;
    }
--- config
    resolver 127.0.0.1:1953 ipv6=off;
    resolver_timeout 100ms;
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call host=api.example.com:8443' acl=hp;
        return 200;
    }
--- no_error_log
dispatch forbidden
[emerg]



=== TEST 33: runtime - bracketed IPv6 target strips brackets and port
--- wasm_modules: hostcalls
--- wasm_acl
    acl v6_allow {
        allow ::1/128;
    }
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/dispatch_http_call host=[::1]:65534' acl=v6_allow;
        return 200;
    }
--- no_error_log
dispatch forbidden
[emerg]
