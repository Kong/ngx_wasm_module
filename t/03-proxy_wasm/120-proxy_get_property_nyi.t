# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

plan tests => repeat_each() * (blocks() * 4);

run_tests();


__DATA__

=== TEST 1: proxy_wasm - get_property() - NYI properties - on_request_headers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: hostcalls
--- http_config eval
qq{
    upstream test_upstream {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location / {
            echo ok;
        }
    }
}
--- config eval
my @vars_nyi = (
    "connection.subject_peer_certificate",
    "connection.dns_san_local_certificate",
    "connection.dns_san_peer_certificate",
    "connection.uri_san_local_certificate",
    "connection.uri_san_peer_certificate",
    "connection.sha256_peer_certificate_digest",
    "upstream.tls_version",
    "upstream.subject_local_certificate",
    "upstream.subject_peer_certificate",
    "upstream.dns_san_local_certificate",
    "upstream.dns_san_peer_certificate",
    "upstream.uri_san_local_certificate",
    "upstream.uri_san_peer_certificate",
    "upstream.sha256_peer_certificate_digest",
    "upstream.local_address",
    "upstream.transport_failure_reason",
    "response.grpc_status",
    "response.trailers",
    "plugin_vm_id"
);

my $vars = CORE::join(',', @vars_nyi);
qq{
    location /t {
        proxy_pass http://test_upstream/;
        proxy_wasm hostcalls 'on=request_headers test=/t/log/properties name=$vars';
    }
};
--- response_body
ok
--- grep_error_log eval: qr/NYI - "(\.|\w)+" property/
--- grep_error_log_out eval
[
    qr/NYI - "connection.subject_peer_certificate" property/,
    qr/NYI - "connection.dns_san_local_certificate" property/,
    qr/NYI - "connection.dns_san_peer_certificate" property/,
    qr/NYI - "connection.uri_san_local_certificate" property/,
    qr/NYI - "connection.uri_san_peer_certificate" property/,
    qr/NYI - "connection.sha256_peer_certificate_digest" property/,
    qr/NYI - "upstream.tls_version" property/,
    qr/NYI - "upstream.subject_local_certificate" property/,
    qr/NYI - "upstream.subject_peer_certificate" property/,
    qr/NYI - "upstream.dns_san_local_certificate" property/,
    qr/NYI - "upstream.dns_san_peer_certificate" property/,
    qr/NYI - "upstream.uri_san_local_certificate" property/,
    qr/NYI - "upstream.uri_san_peer_certificate" property/,
    qr/NYI - "upstream.sha256_peer_certificate_digest" property/,
    qr/NYI - "response.grpc_status" property/,
    qr/NYI - "response.trailers" property/,
    qr/NYI - "plugin_vm_id" property/,
]
--- no_error_log
[error]
