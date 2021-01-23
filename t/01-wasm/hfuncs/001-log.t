# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 3);

add_block_preprocessor(sub {
    my $block = shift;
    my $main_config = <<_EOC_;
        wasm {
            module http_tests $t::TestWasm::crates/rust_http_tests.wasm;
        }
_EOC_

    if (!defined $block->main_config) {
        $block->set_value("main_config", $main_config);
    }
});

run_tests();

__DATA__

=== TEST 1: log: logs in 'log' phase
--- config
    location /t {
        wasm_call log http_tests log_notice_hello;
        return 200;
    }
--- error_log eval
qr/\[notice\] .*? hello world <module: "http_tests", vm: "main", runtime: "\S+"> while logging request/
--- no_error_log
[error]



=== TEST 2: log: logs in 'rewrite' phase
--- config
    location /t {
        wasm_call rewrite http_tests log_notice_hello;
        return 200;
    }
--- error_log eval
qr/\[notice\] .*? hello world <module: "http_tests", vm: "main", runtime: "\S+">, client: 127\.0\.0\.1/
--- no_error_log
[error]
