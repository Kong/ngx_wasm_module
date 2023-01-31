# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 3);

run_tests();

__DATA__

=== TEST 1: environ_sizes_get
--- main_config eval
qq{
    env FOO=bar;
    env NGX_WASI_ENV=1;

    wasm {
        module wasi_host_tests $t::TestWasm::crates/wasi_host_tests.wasm;
    }
}
--- config
    location /t {
        wasm_call rewrite wasi_host_tests test_wasi_environ_sizes_get;
    }
--- response_body_like
envs: [1-9]\d*, size: [1-9]\d*
--- no_error_log
[error]
