# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

our $ExtResolver = $t::TestWasm::extresolver;
our $ExtTimeout = $t::TestWasm::exttimeout;

skip_valgrind();
skip_no_tinygo();

repeat_each(1);

plan tests => repeat_each() * (blocks() * 6);

run_tests();

__DATA__

=== TEST 1: proxy_wasm Go SDK - http_auth_random example
--- timeout eval: $::ExtTimeout
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module go_http_auth_random $ENV{TEST_NGINX_CRATES_DIR}/go_http_auth_random.wasm;
        socket_connect_timeout $::ExtTimeout;
        socket_send_timeout    $::ExtTimeout;
        socket_read_timeout    $::ExtTimeout;
    }
}
--- config eval
qq{
    resolver         $::ExtResolver;
    resolver_timeout $::ExtTimeout;

    location /uuid {
        proxy_wasm go_http_auth_random;
        echo ok;
    }
}
--- request
GET /uuid
--- error_code_like: ^(200|403)$
--- more_headers
key: value
--- error_log eval
[
    qr/\[info\] .*? request header: key: value/,
    qr/\[info\] .*? access (granted|forbidden)/,
    qr/\[info\] .*? response header from httpbin/,
]
--- no_error_log
[error]
[crit]
