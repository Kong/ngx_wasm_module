#ifndef _NGX_WRT_H_INCLUDED_
#define _NGX_WRT_H_INCLUDED_


#include <ngx_core.h>

#include <wavm-c.h>


typedef void *  ngx_wrt_res_t;


ngx_int_t ngx_wrt_wat2wasm(wasm_byte_vec_t *wat, wasm_byte_vec_t *wasm,
    ngx_wrt_res_t **res);

ngx_int_t ngx_wrt_module_new(wasm_engine_t *e, wasm_byte_vec_t *bytes,
    wasm_module_t **out, ngx_wrt_res_t **res);

void ngx_wrt_config_init(wasm_config_t *config);

u_char *ngx_wrt_error_log_handler(ngx_wrt_res_t *res, u_char *buf,
    size_t len);


#endif /* _NGX_WRT_H_INCLUDED_ */
