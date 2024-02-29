# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX;

add_block_preprocessor(sub {
    my $block = shift;
    my $main_config = <<_EOC_;
        wasm {
            module ngx_rust_tests $t::TestWasmX::crates/ngx_rust_tests.wasm;
        }
_EOC_

    if (!defined $block->main_config) {
        $block->set_value("main_config", $main_config);
    }
});

plan_tests(3);
run_tests();

__DATA__

=== TEST 1: log: logs in 'log' phase
--- valgrind
--- config
    location /t {
        wasm_call log ngx_rust_tests log_notice_hello;
        return 200;
    }
--- error_log eval
qr/\[notice\] .*? hello world <module: "ngx_rust_tests", vm: "main", runtime: "\S+"> while logging request/
--- no_error_log
[error]



=== TEST 2: log: logs in 'rewrite' phase
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests log_notice_hello;
        return 200;
    }
--- error_log eval
qr/\[notice\] .*? hello world <module: "ngx_rust_tests", vm: "main", runtime: "\S+">, client: 127\.0\.0\.1/
--- no_error_log
[error]
