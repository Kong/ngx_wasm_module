#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wrt.h>

static ngx_str_t wamr_no_wat = ngx_string("WAMR has no support for wat to wasm compilation");


wasm_config_t *
ngx_wrt_config_create()
{
  wasm_config_t *wasm_config = wasm_config_new();

  /* WAMR doesn't have any tweakable config at this point */
  return wasm_config;
}


ngx_int_t
ngx_wrt_config_init(ngx_log_t *log, wasm_config_t *wasm_config,
                    ngx_wavm_conf_t *vm_config)
{
  if (vm_config->compiler.len) {
    ngx_wavm_log_error(NGX_LOG_ERR, log, NULL,
                       "wamr does not support the compiler directive",
                       &vm_config->compiler);
    return NGX_ERROR;
  }
  
  return NGX_OK;
}


ngx_int_t
ngx_wrt_engine_new(wasm_config_t *config, wasm_engine_t **out,
                   ngx_wavm_err_t *err)
{
  *out = wasm_engine_new_with_config(config);
  if (*out == NULL) {
    return NGX_ERROR;
  }

  return NGX_OK;
}


ngx_int_t
ngx_wrt_wat2wasm(wasm_byte_vec_t *wat, wasm_byte_vec_t *wasm,
                 ngx_wavm_err_t *err)
{
  err->res = &wamr_no_wat;
  return NGX_ERROR;
}


ngx_int_t
ngx_wrt_module_validate(wasm_store_t *s, wasm_byte_vec_t *bytes,
                        ngx_wavm_err_t *err)
{
  if (!wasm_module_validate(s, bytes)) {
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
    return NGX_ERROR;
  }

  return NGX_OK;
}

ngx_int_t
ngx_wrt_instance_new(wasm_store_t *s, wasm_module_t *m,
                     wasm_extern_vec_t *imports, wasm_instance_t **instance,
                     ngx_wavm_err_t *err)
{
  *instance = wasm_instance_new(s, (const wasm_module_t *)m,
                                (const wasm_extern_vec_t *)imports,
                                &err->trap);
  if (*instance == NULL) {
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

  return NGX_OK;
}

u_char *
ngx_wrt_error_log_handler(ngx_wrt_res_t *res, u_char *buf, size_t len)
{
  u_char *p = buf;

  if (res->len) {
    p = ngx_snprintf(buf, ngx_min(len, res->len), "%V", res);

    if (res != &wamr_no_wat) {
      ngx_free(res->data);
      ngx_free(res);
    }
  }

  return p;
}
