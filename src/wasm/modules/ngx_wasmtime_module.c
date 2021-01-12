#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_core.h>
#include <ngx_wasm_core.h>

#include <wasmtime.h>




static ngx_int_t
ngx_wasmtime_init(ngx_wasm_vm_t *vm)
{
    //wasmtime_config_max_wasm_stack_set(config, (size_t) 125000 * 5);
    //wasmtime_config_static_memory_maximum_size_set(engine->config, 0);
    //wasmtime_config_profiler_set(engine->config, WASMTIME_PROFILING_STRATEGY_JITDUMP);
    //wasmtime_config_debug_info_set(engine->config, 1);

    return NGX_OK;
}


/*
static void
ngx_wasmtime_shutdown(ngx_wasm_vm_t *vm)
{

}
*/


static ngx_int_t
ngx_wasmtime_wat2wasm(ngx_wasm_vm_t *vm, u_char *wat, size_t len,
    wasm_byte_vec_t *bytes, ngx_wrt_error_pt *err)
{
    wasm_byte_vec_t   wat_bytes;

    wasm_byte_vec_new(&wat_bytes, len, (const char *) wat);

    *err = wasmtime_wat2wasm(&wat_bytes, bytes);

    wasm_byte_vec_delete(&wat_bytes);

    if (*err) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_wasmtime_module_new(ngx_wasm_vm_module_t *module,
    wasm_byte_vec_t *bytes, ngx_wrt_error_pt *err)
{
    ngx_wasm_vm_t  *vm = module->vm;

    *err = wasmtime_module_new(vm->engine, bytes, &module->module);

    if (*err) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_wasmtime_instance_new(ngx_wasm_vm_instance_t *instance,
    wasm_trap_t **trap, ngx_wrt_error_pt *err)
{
    *err = wasmtime_instance_new(instance->store, instance->module->module,
                             (const wasm_extern_t * const *) instance->env.data,
                             instance->env.size, &instance->instance, trap);
    if (*err || *trap) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_wasmtime_func_call(wasm_func_t *func, wasm_trap_t **trap,
    ngx_wrt_error_pt *err)
{
    *err = wasmtime_func_call(func, NULL, 0, NULL, 0, trap);
    if (*err || *trap) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static u_char *
ngx_wasmtime_error_log_handler(ngx_wrt_error_pt err, u_char *buf, size_t len)
{
    u_char            *p = buf;
    wasm_message_t     error_msg;
    wasmtime_error_t  *error = err;

    if (error) {
        ngx_memzero(&error_msg, sizeof(wasm_message_t));
        wasmtime_error_message(error, &error_msg);

        p = ngx_snprintf(buf, len, " (%*s)", error_msg.size, error_msg.data);

        wasm_byte_vec_delete(&error_msg);
        wasmtime_error_delete(error);
    }

    return p;
}


/* ngx_wasmtime_module */


static ngx_wrt_t  ngx_wasmtime_runtime = {
    ngx_string("wasmtime"),
    ngx_wasmtime_init,
    NULL,
    ngx_wasmtime_wat2wasm,
    ngx_wasmtime_module_new,
    ngx_wasmtime_instance_new,
    ngx_wasmtime_func_call,
    ngx_wasmtime_error_log_handler
};


static ngx_wasm_module_t  ngx_wasmtime_module_ctx = {
    &ngx_wasmtime_runtime,               /* runtime */
    NULL,                                /* hdefs */
    NULL,                                /* create configuration */
    NULL,                                /* init configuration */
    NULL,                                /* init module */
};


ngx_module_t  ngx_wasmtime_module = {
    NGX_MODULE_V1,
    &ngx_wasmtime_module_ctx,            /* module context */
    NULL,                                /* module directives */
    NGX_WASM_MODULE,                     /* module type */
    NULL,                                /* init master */
    NULL,                                /* init module */
    NULL,                                /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};
