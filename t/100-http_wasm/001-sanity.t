# vim:set ft= ts=4 sw=4 et fdm=marker:
use lib '.';
use t::TestWasm;

plan tests => repeat_each() * (blocks() * 3);

add_block_preprocessor(sub {
    my $block = shift;

    if (!defined $block->no_error_log) {
        $block->set_value("no_error_log", "[error]");
    }
});

run_tests();

__DATA__

=== TEST 1: wasm_call_log directive: no wasm{} configuration block
--- main_config
--- config
    location /t {
        wasm_call_log hello get;
        return 200;
    }
--- error_log eval
qr/\[emerg\] .*? no "wasm" section in configuration/
--- must_die
