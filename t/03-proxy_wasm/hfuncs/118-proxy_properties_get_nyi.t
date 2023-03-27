# vim:set ft= ts=4 sts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind('wasmtime');

plan tests => repeat_each() * (blocks() * 4);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - get_property() - NYI properties on: request_headers
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
my $vars = CORE::join(',', qw(
    connection.subject_peer_certificate
    connection.dns_san_local_certificate
    connection.dns_san_peer_certificate
    connection.uri_san_local_certificate
    connection.uri_san_peer_certificate
    connection.sha256_peer_certificate_digest
    upstream.tls_version
    upstream.subject_local_certificate
    upstream.subject_peer_certificate
    upstream.dns_san_local_certificate
    upstream.dns_san_peer_certificate
    upstream.uri_san_local_certificate
    upstream.uri_san_peer_certificate
    upstream.sha256_peer_certificate_digest
    upstream.local_address
    upstream.transport_failure_reason
    response.grpc_status
    response.trailers
    plugin_vm_id
));

qq{
    location /t {
        proxy_pass http://test_upstream/;
        proxy_wasm hostcalls 'on=request_headers \
                              test=/t/log/properties \
                              name=$vars';
    }
};
--- response_body
ok
--- grep_error_log eval: qr/NYI - "(\.|\w)+" property/
--- grep_error_log_out eval
qr/NYI - "connection.subject_peer_certificate" property
NYI - "connection.dns_san_local_certificate" property
NYI - "connection.dns_san_peer_certificate" property
NYI - "connection.uri_san_local_certificate" property
NYI - "connection.uri_san_peer_certificate" property
NYI - "connection.sha256_peer_certificate_digest" property
NYI - "upstream.tls_version" property
NYI - "upstream.subject_local_certificate" property
NYI - "upstream.subject_peer_certificate" property
NYI - "upstream.dns_san_local_certificate" property
NYI - "upstream.dns_san_peer_certificate" property
NYI - "upstream.uri_san_local_certificate" property
NYI - "upstream.uri_san_peer_certificate" property
NYI - "upstream.sha256_peer_certificate_digest" property
NYI - "upstream.local_address" property
NYI - "upstream.transport_failure_reason" property
NYI - "response.grpc_status" property
NYI - "response.trailers" property
NYI - "plugin_vm_id" property/
--- no_error_log
[error]
