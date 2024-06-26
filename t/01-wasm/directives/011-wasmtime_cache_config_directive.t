# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

if ($t::TestWasmX::nginxV !~ m/wasmtime/) {
    plan(skip_all => "not built with Wasmtime, skipping");

} else {
    plan_tests(4);
}

run_tests();

__DATA__

=== TEST 1: wasmtime cache_config - missing file
--- main_config
    wasm {
        wasmtime {
            cache_config missing_file;
        }
    }
--- error_log eval
qr/\[emerg\] .*? failed configuring wasmtime cache; failed to read config file: missing_file/
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 2: wasmtime cache_config - invalid contents file
--- user_files
>>> wasmtime_config.toml
invalid contents
--- main_config
    wasm {
        wasmtime {
            cache_config $TEST_NGINX_HTML_DIR/wasmtime_config.toml;
        }
    }
--- error_log eval
qr@\[emerg\] .*? failed configuring wasmtime cache; failed to parse config file: .*/wasmtime_config.toml@
--- no_error_log
[error]
[crit]
--- must_die



=== TEST 3: wasmtime cache_config - valid file, cache disabled
--- user_files
>>> wasmtime_config.toml
[cache]
enabled = false
--- main_config eval
qq{
    wasm {
        module hostcalls $t::TestWasmX::crates/hostcalls.wasm;

        wasmtime {
            cache_config $ENV{TEST_NGINX_HTML_DIR}/wasmtime_config.toml;
        }
    }
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_request_header \
                              value=Hello:wasm';
        proxy_wasm hostcalls 'test=/t/echo/headers';
    }
--- response_body
Host: localhost
Connection: close
Hello: wasm
--- error_log eval
qr@setting wasmtime cache config file: ".*/wasmtime_config.toml"@
--- no_error_log
[error]



=== TEST 4: wasmtime cache_config - valid file, cache enabled
--- user_files
>>> wasmtime_config.toml
[cache]
enabled = true
directory = "/tmp/ngx_wasm_module/cache/wasmtime"
--- main_config eval
qq{
    wasm {
        module hostcalls $t::TestWasmX::crates/hostcalls.wasm;

        wasmtime {
            cache_config $ENV{TEST_NGINX_HTML_DIR}/wasmtime_config.toml;
        }
    }
}
--- config
    location /t {
        proxy_wasm hostcalls 'test=/t/set_request_header \
                              value=Hello:wasm';
        proxy_wasm hostcalls 'test=/t/echo/headers';
    }
--- response_body
Host: localhost
Connection: close
Hello: wasm
--- error_log eval
qr@setting wasmtime cache config file: ".*/wasmtime_config.toml"@
--- no_error_log
[error]
