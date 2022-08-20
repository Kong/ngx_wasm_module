#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_ssl.h>


/* Modified from `ngx_ssl_trusted_certificate` */
ngx_int_t
ngx_wasm_trusted_certificate(ngx_ssl_t *ssl, ngx_str_t *cert,
    ngx_int_t depth)
{
    SSL_CTX_set_verify(ssl->ctx, SSL_CTX_get_verify_mode(ssl->ctx),
                       ngx_wasm_ssl_verify_callback);

    SSL_CTX_set_verify_depth(ssl->ctx, depth);

    if (SSL_CTX_load_verify_locations(ssl->ctx, (char *) cert->data, NULL)
        == 0)
    {
        ngx_ssl_error(NGX_LOG_EMERG, ssl->log, 0,
                      "SSL_CTX_load_verify_locations(\"%s\") failed",
                      cert->data);
        return NGX_ERROR;
    }

    /*
     * SSL_CTX_load_verify_locations() may leave errors in the error queue
     * while returning success
     */

    ERR_clear_error();

    return NGX_OK;
}


int
ngx_wasm_ssl_verify_callback(int ok, X509_STORE_CTX *x509_store)
{
    return 1;
}
