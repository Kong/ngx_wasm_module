# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasmX::Lua;

our $dyn = $ENV{NGX_BUILD_DYNAMIC_MODULE} || 0;

skip_no_openresty();

plan_tests(4);
run_tests();

__DATA__

=== TEST 1: OpenResty + dyn ngx_wasmx_module - Swap necessary modules when loaded in OpenResty
See notes in ngx_wasm_core_module.c
--- skip_eval: 4: $::dyn != 1
--- load_nginx_modules: ngx_http_headers_more_filter_module
--- wasm_modules: on_phases
--- ignore_response_body
--- grep_error_log eval: qr/\[notice\] .*? swapping modules.*/
--- grep_error_log_out eval
qr/\[notice\] .*? swapping modules: "ngx_http_headers_more_filter_module" .*? and "ngx_http_wasm_filter_module".*
\[notice\] .*? swapping modules: "ngx_http_lua_module" .*? and "ngx_wasm_core_module"/
--- no_error_log
[error]
[crit]
