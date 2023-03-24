# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind('wasmtime');

my $n = 4;
plan tests => repeat_each() * (blocks() * $n);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - trap on_vm_start
Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]

'daemon off' must be set to check exit_code is 2
Valgrind mode already writes 'daemon off'
HUP mode does not catch the worker exit_code
--- skip_eval: $n: $ENV{TEST_NGINX_USE_HUP} == 1
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm 'do_trap';
    }
}.(defined $ENV{TEST_NGINX_USE_VALGRIND} ? '' : 'daemon off;')
--- config
    location /t {
        proxy_wasm hostcalls;
        return 200;
    }
--- error_log eval
[
    qr/\[crit\] .*? panicked at 'trap on_vm_start'/,
    qr/(\[error\] .*?(Uncaught RuntimeError: )?unreachable|wasm trap: wasm `unreachable` instruction executed)/,
    qr/\[emerg\] .*? proxy_wasm failed initializing "hostcalls" filter \(initialization failed\)/,
]
--- must_die: 2



=== TEST 2: proxy_wasm - return false on_vm_start
'daemon off' must be set to check exit_code is 2
Valgrind mode already writes 'daemon off'
HUP mode does not catch the worker exit_code
--- skip_eval: $n: $ENV{TEST_NGINX_USE_HUP} == 1
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm 'do_false';
    }
}.(defined $ENV{TEST_NGINX_USE_VALGRIND} ? '' : 'daemon off;')
--- config
    location /t {
        proxy_wasm hostcalls;
        return 200;
    }
--- error_log eval
[
    qr/\[info\] .*? on_vm_start returning false/,
    qr/\[emerg\] .*? proxy_wasm failed initializing "hostcalls" filter \(initialization failed\)/,
]
--- no_error_log
[crit]
--- must_die: 2



=== TEST 3: proxy_wasm - trap on_configure
Wasmtime trap format:
    [error] error while executing ...
    [stacktrace]
    Caused by:
        [trap msg]

Wasmer trap format:
    [error] [trap msg]

V8 trap format:
    [error] Uncaught RuntimeError: [trap msg]

'daemon off' must be set to check exit_code is 2
Valgrind mode already writes 'daemon off'
HUP mode does not catch the worker exit_code
--- skip_eval: $n: $ENV{TEST_NGINX_USE_HUP} == 1
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
    }
}.(defined $ENV{TEST_NGINX_USE_VALGRIND} ? '' : 'daemon off;')
--- config
    location /t {
        proxy_wasm hostcalls 'on_configure=do_trap';
        return 200;
    }
--- error_log eval
[
    qr/\[crit\] .*? panicked at 'trap on_configure'/,
    qr/(\[error\] .*?(Uncaught RuntimeError: )?unreachable|wasm trap: wasm `unreachable` instruction executed)/,
    qr/\[emerg\] .*? proxy_wasm failed initializing "hostcalls" filter \(initialization failed\)/,
]
--- must_die: 2



=== TEST 4: proxy_wasm - return false on_configure
'daemon off' must be set to check exit_code is 2
Valgrind mode already writes 'daemon off'
HUP mode does not catch the worker exit_code
--- skip_eval: $n: $ENV{TEST_NGINX_USE_HUP} == 1
--- main_config eval
qq{
    wasm {
        module hostcalls $ENV{TEST_NGINX_CRATES_DIR}/hostcalls.wasm;
    }
}.(defined $ENV{TEST_NGINX_USE_VALGRIND} ? '' : 'daemon off;')
--- config
    location /t {
        proxy_wasm hostcalls 'on_configure=do_return_false';
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? proxy_wasm failed initializing "hostcalls" filter \(initialization failed\)/
--- no_error_log
[error]
[crit]
--- must_die: 2
