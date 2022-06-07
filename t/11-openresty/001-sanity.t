# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

our $nginxV = $t::TestWasm::nginxV;

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: OpenResty sanity
--- skip_eval: 4: $::nginxV !~ m/openresty/
--- config
    location /t {
        content_by_lua_block {
            ngx.exit(403)
        }
    }
--- error_code: 403
--- response_body_like: 403 Forbidden
--- no_error_log
[error]
[crit]



=== TEST 2: ngx_wasm_module sanity
--- skip_eval: 4: $::nginxV !~ m/openresty/
--- main_config
    wasm {}
--- config
    location /t {
        content_by_lua_block {
            ngx.exit(403)
        }
    }
--- error_code: 403
--- response_body_like: 403 Forbidden
--- error_log
[wasm] "main" wasm VM initialized
--- no_error_log
[error]
