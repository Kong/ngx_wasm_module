#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wrt.h>


static void ngx_wasmer_last_err(ngx_wrt_res_t **res);


wasm_config_t *
ngx_wrt_config_create()
{
    wasm_config_t  *wasm_config = wasm_config_new();

    //wasm_config_set_compiler(wasm_config, SINGLEPASS);
    //wasm_config_set_engine(config, UNIVERSAL);

    return wasm_config;
}


ngx_int_t
ngx_wrt_config_init(ngx_log_t *log, wasm_config_t *wasm_config,
    ngx_wavm_conf_t *vm_config)
{
    if (vm_config->compiler.len) {
        if (ngx_strncmp(vm_config->compiler.data, "cranelift", 9) == 0) {
            wasm_config_set_compiler(wasm_config, CRANELIFT);

        } else if (ngx_strncmp(vm_config->compiler.data, "singlepass", 10) == 0) {
            wasm_config_set_compiler(wasm_config, SINGLEPASS);

        } else if (ngx_strncmp(vm_config->compiler.data, "llvm", 4) == 0) {
            wasm_config_set_compiler(wasm_config, LLVM);

        } else {
            ngx_wavm_log_error(NGX_LOG_ERR, log, NULL,
                               "invalid compiler \"%V\"",
                               &vm_config->compiler);
            return NGX_ERROR;
        }

        ngx_wavm_log_error(NGX_LOG_INFO, log, NULL,
                           "using wasmer with compiler: \"%V\"",
                           &vm_config->compiler);
    }

    return NGX_OK;
}


ngx_int_t
ngx_wrt_engine_new(wasm_config_t *config, wasm_engine_t **out,
    ngx_wavm_err_t *err)
{
    *out = wasm_engine_new_with_config(config);
    if (*out == NULL) {
        ngx_wasmer_last_err(&err->res);
        return NGX_ERROR;
    }

    return NGX_OK;
}


ngx_int_t
ngx_wrt_wat2wasm(wasm_byte_vec_t *wat, wasm_byte_vec_t *wasm,
    ngx_wavm_err_t *err)
{
    wat2wasm(wat, wasm);
    if (wasm->data == NULL) {
        ngx_wasmer_last_err(&err->res);
        return NGX_ERROR;
    }

    return NGX_OK;
}


ngx_int_t
ngx_wrt_module_validate(wasm_store_t *s, wasm_byte_vec_t *bytes,
    ngx_wavm_err_t *err)
{
    if (!wasm_module_validate(s, bytes)) {
        ngx_wasmer_last_err(&err->res);
        return NGX_ERROR;
    }

    return NGX_OK;
}


ngx_int_t
ngx_wrt_module_new(wasm_store_t *s, wasm_byte_vec_t *bytes,
    wasm_module_t **out, ngx_wavm_err_t *err)
{
    *out = wasm_module_new(s, bytes);
    if (*out == NULL) {
        ngx_wasmer_last_err(&err->res);
        return NGX_ERROR;
    }

    return NGX_OK;
}


ngx_int_t
ngx_wrt_instance_new(wasm_store_t *s, wasm_module_t *m,
    wasm_extern_vec_t *imports, wasm_instance_t **instance,
    ngx_wavm_err_t *err)
{
    *instance = wasm_instance_new(s, m, (const wasm_extern_vec_t *) imports,
                                  &err->trap);
    if (*instance == NULL) {
        ngx_wasmer_last_err(&err->res);
        return NGX_ERROR;
    }

    return NGX_OK;
}


ngx_int_t
ngx_wrt_func_call(wasm_func_t *f, wasm_val_vec_t *args, wasm_val_vec_t *rets,
    ngx_wavm_err_t *err)
{
    err->trap = wasm_func_call(f, args, rets);
    if (err->trap) {
        return NGX_ERROR;
    }

    if (wasmer_last_error_length()) {
        ngx_wasmer_last_err(&err->res);
        return NGX_ERROR;
    }

    return NGX_OK;
}


u_char *
ngx_wrt_error_log_handler(ngx_wrt_res_t *res, u_char *buf, size_t len)
{
    u_char  *p = buf;

    if (res->len) {
        p = ngx_snprintf(buf, ngx_min(len, res->len), "%V", res);

        ngx_free(res->data);
        ngx_free(res);
    }

    return p;
}


static void
ngx_wasmer_last_err(ngx_wrt_res_t **res)
{
    ngx_str_t   *werr = NULL;
    u_char      *err = (u_char *) "no memory";
    ngx_uint_t   errlen;
    ngx_int_t    rc;

    errlen = wasmer_last_error_length();
    if (errlen) {
        werr = ngx_alloc(sizeof(ngx_str_t), ngx_cycle->log);
        if (werr == NULL) {
            goto error;
        }

        werr->len = errlen;
        werr->data = ngx_alloc(errlen, ngx_cycle->log);
        if (werr->data == NULL) {
            goto error;
        }

        rc = wasmer_last_error_message((char *) werr->data, errlen);
        if (rc < 0) {
            err = (u_char *) "NYI: wasmer error";
            goto error;
        }

        *res = werr;
    }
#if (NGX_DEBUG)
    else {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0,
                      "no wasmer error to retrieve");
    }
#endif

    return;

error:

    ngx_log_error(NGX_LOG_CRIT, ngx_cycle->log, 0, NULL,
                  "failed to retrieve last wasmer error: %s", err);

    if (werr) {
        if (werr->data) {
            ngx_free(werr->data);
        }

        ngx_free(werr);
    }
}
