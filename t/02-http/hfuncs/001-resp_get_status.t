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

=== TEST 1: resp_get_status: 'rewrite' phase
--- config
    location /t {
        wasm_call rewrite ngx_rust_tests log_resp_status;
        return 200;
    }
--- error_log
resp status: 0
--- no_error_log
[error]



=== TEST 2: resp_get_status: 'content' phase
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /t {
        wasm_call content ngx_rust_tests log_resp_status;
        echo ok;
    }
--- error_log
resp status: 0
--- no_error_log
[error]



=== TEST 3: resp_get_status: gets status in 'log' phase
--- config
    location /t {
        wasm_call log ngx_rust_tests log_resp_status;
        return 200;
    }
--- error_log eval
qr/\[notice\] .*? resp status: 200/
--- no_error_log
[error]
