#ifndef _NGX_WASM_SSL_H_INCLUDED_
#define _NGX_WASM_SSL_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wrt.h>


typedef struct ngx_wasm_ssl_conf_s {
    ngx_str_t       trusted_certificate;
    ngx_ssl_t       ssl;

    ngx_flag_t      verify_cert;
    ngx_flag_t      verify_host;
    ngx_flag_t      no_verify_warn;
} ngx_wasm_ssl_conf_t;


ngx_wasm_ssl_conf_t *ngx_wasm_core_ssl_conf(ngx_cycle_t *cycle);

ngx_int_t ngx_wasm_trusted_certificate(ngx_ssl_t *ssl,
    ngx_str_t *cert, ngx_int_t depth);
int ngx_wasm_ssl_verify_callback(int ok, X509_STORE_CTX *x509_store);


#endif /* _NGX_WASM_SSL_H_INCLUDED_ */
