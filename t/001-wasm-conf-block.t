# vim:set ft= ts=4 sw=4 et fdm=marker:
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 2);

run_tests();

__DATA__

=== TEST 1: NGINX supports a wasm{} configuration block
--- main_config
    wasm {}
--- config
--- no_error_log
[error]



=== TEST 2: detects duplicated wasm{} blocks
--- main_config
    wasm {}
    wasm {}
--- config
--- must_die
--- error_log
"wasm" directive is duplicate
