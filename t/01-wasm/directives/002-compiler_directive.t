# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind('always');

our $nginxV = $t::TestWasm::nginxV;

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: compiler directive - invalid arguments
--- main_config
    wasm {
        compiler arg1 arg2;
    }
--- error_log eval
qr/\[emerg\] .*? invalid number of arguments in "compiler" directive/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 2: compiler directive - bad compiler
--- main_config
    wasm {
        compiler unknown;
    }
--- error_log eval
[
    qr/\[error\] .*? \[wasm\] invalid compiler "unknown"/,
    qr/\[emerg\] .*? \[wasm\] failed initializing wasm VM: invalid configuration/,
]
--- no_error_log
[alert]
--- must_die



=== TEST 3: compiler directive - default (unspecified)
--- main_config
    wasm {}
--- error_log eval
qr/\[info\] .*? \[wasm\] using \S+ with compiler: "auto"/
--- no_error_log
[error]
[emerg]



=== TEST 4: compiler directive - wasmtime 'auto'
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        compiler auto;
    }
--- error_log eval
qr/\[info\] .*? \[wasm\] using wasmtime with compiler: "auto"/
--- no_error_log
[error]
[emerg]



=== TEST 5: compiler directive - wasmtime 'cranelift'
--- skip_eval: 4: $::nginxV !~ m/wasmtime/
--- main_config
    wasm {
        compiler cranelift;
    }
--- error_log eval
qr/\[info\] .*? \[wasm\] using wasmtime with compiler: "cranelift"/
--- no_error_log
[error]
[emerg]



=== TEST 6: compiler directive - wasmer 'auto'
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        compiler auto;
    }
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] wasmer "auto" compiler selected "\S+"/,
    qr/\[info\] .*? \[wasm\] using wasmer with compiler: "auto"/
]
--- no_error_log
[emerg]



=== TEST 7: compiler directive - wasmer 'cranelift'
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        compiler cranelift;
    }
--- error_log eval
qr/\[info\] .*? \[wasm\] using wasmer with compiler: "cranelift"/
--- no_error_log
[error]
[emerg]



=== TEST 8: compiler directive - wasmer 'singlepass'
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        compiler singlepass;
    }
--- error_log eval
qr/\[info\] .*? \[wasm\] using wasmer with compiler: "singlepass"/
--- no_error_log
[error]
[emerg]



=== TEST 9: compiler directive - wasmer 'llvm'
--- SKIP
--- skip_eval: 4: $::nginxV !~ m/wasmer/
--- main_config
    wasm {
        compiler llvm;
    }
--- error_log eval
qr/\[info\] .*? \[wasm\] using wasmer with compiler: "llvm"/
--- no_error_log
[error]
[emerg]
