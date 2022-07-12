#ifndef _NGX_WASM_SSL_H_INCLUDED_
#define _NGX_WASM_SSL_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wrt.h>


typedef struct ngx_wasm_ssl_conf_s {
    ngx_str_t       trusted_certificate;
    ngx_ssl_t       ssl;

    ngx_flag_t      skip_verify;
    ngx_flag_t      skip_host_check;
} ngx_wasm_ssl_conf_t;


ngx_wasm_ssl_conf_t *ngx_wasm_ssl_conf(ngx_cycle_t *cycle);


#endif /* _NGX_WASM_SSL_H_INCLUDED_ */
