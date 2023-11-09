# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

our $ExtResolver = $t::TestWasm::extresolver;
our $ExtTimeout = $t::TestWasm::exttimeout;

skip_valgrind();
skip_no_go_sdk();

repeat_each(1);

plan tests => repeat_each() * (blocks() * 6);

run_tests();

__DATA__

=== TEST 1: proxy_wasm Go SDK - http_auth_random example
--- skip_eval: 6: defined $ENV{TEST_NGINX_RANDOMIZE} && $ENV{TEST_NGINX_RANDOMIZE}
--- timeout eval: $::ExtTimeout
--- load_nginx_modules: ngx_http_echo_module
--- main_config eval
qq{
    wasm {
        module go_http_auth_random $ENV{TEST_NGINX_CRATES_DIR}/go_http_auth_random.wasm;
    }
}
--- http_config
server {
    listen       2000;
    server_name  httpbin;

    location /uuid {
        # Generating a uuid seems excessive; $request_id has some
        # randomness on a distribution of runs of this test.
        echo $request_id;
    }
}
--- config eval
qq{
    resolver         $::ExtResolver;
    resolver_add     127.0.0.1 httpbin;

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
