#ifndef _NGX_WRT_H_INCLUDED_
#define _NGX_WRT_H_INCLUDED_


#include <ngx_core.h>

#include <wasmer_wasm.h>


typedef ngx_str_t  ngx_wrt_res_t;


void ngx_wasmer_last_err(ngx_wrt_res_t **res);


static ngx_inline void
ngx_wrt_config_init(wasm_config_t *config)
{
    //wasm_config_set_compiler(config, LLVM);
    //wasm_config_set_engine(config, NATIVE);
}


static ngx_inline ngx_int_t
ngx_wrt_wat2wasm(wasm_byte_vec_t *wat, wasm_byte_vec_t *wasm,
    ngx_wrt_res_t **res)
{
    wat2wasm(wat, wasm);
    if (wasm->data == NULL) {
        ngx_wasmer_last_err(res);
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_inline ngx_int_t
ngx_wrt_module_new(wasm_store_t *s, wasm_byte_vec_t *bytes,
    wasm_module_t **out, ngx_wrt_res_t **res)
{
    *out = wasm_module_new(s, bytes);
    if (*out == NULL) {
        ngx_wasmer_last_err(res);
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_inline ngx_int_t
ngx_wrt_instance_new(wasm_store_t *s, wasm_module_t *m,
    wasm_extern_vec_t *imports, wasm_instance_t **instance,
    wasm_trap_t **trap, ngx_wrt_res_t **res)
{
    *instance = wasm_instance_new(s, m, (const wasm_extern_vec_t *) imports,
                                  trap);
    if (*instance == NULL) {
        ngx_wasmer_last_err(res);
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_inline ngx_int_t
ngx_wrt_func_call(wasm_func_t *f, wasm_val_vec_t *args, wasm_val_vec_t *rets,
    wasm_trap_t **trap, ngx_wrt_res_t **res)
{
    wasm_val_vec_t  a = WASM_EMPTY_VEC, b = WASM_EMPTY_VEC;

    *trap = wasm_func_call(f, &a, &b);
    if (*trap) {
        return NGX_ERROR;
    }

    if (wasmer_last_error_length()) {
        ngx_wasmer_last_err(res);
        return NGX_ERROR;
    }

    return NGX_OK;
}


u_char *ngx_wrt_error_log_handler(ngx_wrt_res_t *res, u_char *buf,
    size_t len);


#endif /* _NGX_WRT_H_INCLUDED_ */
