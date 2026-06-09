# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

plan tests => 41;
run_tests();

__DATA__

=== TEST 1: get_upstream_configuration - no upstream parameter set
--- wasm_modules: hostcalls
--- config
    location /t {
        proxy_wasm hostcalls;
        return 200;
    }
--- error_log
no upstream config
--- no_error_log
[emerg]



=== TEST 2: get_upstream_configuration - upstream not found
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 0.0.0.0;
    }
--- config
    location /t {
        proxy_wasm hostcalls upstream=nonexistent;
        return 200;
    }
--- error_log
no upstream config
--- no_error_log
[emerg]



=== TEST 3: get_upstream_configuration - single server with defaults
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 0.0.0.0;
    }
--- config
    location /t {
        proxy_wasm hostcalls upstream=test_upstream;
        return 200;
    }
--- error_log
[hostcalls] upstream: 0.0.0.0:80 weight=1 max_fails=1 fail_timeout=10 backup=false
--- no_error_log
[emerg]



=== TEST 4: get_upstream_configuration - single server with custom port
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8891;
    }
--- config
    location /t {
        proxy_wasm hostcalls upstream=test_upstream;
        return 200;
    }
--- error_log
[hostcalls] upstream: 127.0.0.1:8891 weight=1 max_fails=1 fail_timeout=10 backup=false
--- no_error_log
[emerg]



=== TEST 5: get_upstream_configuration - multiple servers
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8891;
        server 127.0.0.1:8892;
        server 127.0.0.1:8893;
    }
--- config
    location /t {
        proxy_wasm hostcalls upstream=test_upstream;
        return 200;
    }
--- error_log
[hostcalls] upstream: 127.0.0.1:8891 weight=1 max_fails=1 fail_timeout=10 backup=false
[hostcalls] upstream: 127.0.0.1:8892 weight=1 max_fails=1 fail_timeout=10 backup=false
[hostcalls] upstream: 127.0.0.1:8893 weight=1 max_fails=1 fail_timeout=10 backup=false
--- no_error_log
[emerg]



=== TEST 6: get_upstream_configuration - custom weight
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8891 weight=5;
        server 127.0.0.1:8892 weight=3;
    }
--- config
    location /t {
        proxy_wasm hostcalls upstream=test_upstream;
        return 200;
    }
--- error_log
[hostcalls] upstream: 127.0.0.1:8891 weight=5 max_fails=1 fail_timeout=10 backup=false
[hostcalls] upstream: 127.0.0.1:8892 weight=3 max_fails=1 fail_timeout=10 backup=false
--- no_error_log
[emerg]



=== TEST 7: get_upstream_configuration - custom max_fails
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8891 max_fails=5;
        server 127.0.0.1:8892 max_fails=0;
    }
--- config
    location /t {
        proxy_wasm hostcalls upstream=test_upstream;
        return 200;
    }
--- error_log
[hostcalls] upstream: 127.0.0.1:8891 weight=1 max_fails=5 fail_timeout=10 backup=false
[hostcalls] upstream: 127.0.0.1:8892 weight=1 max_fails=0 fail_timeout=10 backup=false
--- no_error_log
[emerg]



=== TEST 8: get_upstream_configuration - custom fail_timeout
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8891 fail_timeout=30;
        server 127.0.0.1:8892 fail_timeout=0;
    }
--- config
    location /t {
        proxy_wasm hostcalls upstream=test_upstream;
        return 200;
    }
--- error_log
[hostcalls] upstream: 127.0.0.1:8891 weight=1 max_fails=1 fail_timeout=30 backup=false
[hostcalls] upstream: 127.0.0.1:8892 weight=1 max_fails=1 fail_timeout=0 backup=false
--- no_error_log
[emerg]



=== TEST 9: get_upstream_configuration - backup server
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8891;
        server 127.0.0.1:8892 backup;
    }
--- config
    location /t {
        proxy_wasm hostcalls upstream=test_upstream;
        return 200;
    }
--- error_log
[hostcalls] upstream: 127.0.0.1:8891 weight=1 max_fails=1 fail_timeout=10 backup=false
[hostcalls] upstream: 127.0.0.1:8892 weight=1 max_fails=1 fail_timeout=10 backup=true
--- no_error_log
[emerg]



=== TEST 10: get_upstream_configuration - down server is skipped
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8891;
        server 127.0.0.1:8892 down;
        server 127.0.0.1:8893;
    }
--- config
    location /t {
        proxy_wasm hostcalls upstream=test_upstream;
        return 200;
    }
--- error_log
[hostcalls] upstream: 127.0.0.1:8891 weight=1 max_fails=1 fail_timeout=10 backup=false
[hostcalls] upstream: 127.0.0.1:8893 weight=1 max_fails=1 fail_timeout=10 backup=false
--- no_error_log
[hostcalls] upstream: 127.0.0.1:8892
[emerg]



=== TEST 11: get_upstream_configuration - all servers down returns empty
--- wasm_modules: hostcalls
--- http_config
    upstream test_upstream {
        server 127.0.0.1:8891 down;
        server 127.0.0.1:8892 down;
    }
--- config
    location /t {
        proxy_wasm hostcalls upstream=test_upstream;
        return 200;
    }
--- no_error_log
[hostcalls] upstream:
[emerg]
