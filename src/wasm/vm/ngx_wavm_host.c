#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wavm.h>


static const wasm_valkind_t  ngx_wavm_i32 = WASM_I32;
static const wasm_valkind_t  ngx_wavm_i64 = WASM_I64;


const wasm_valkind_t *ngx_wavm_arity_i32[] = {
    &ngx_wavm_i32,
    NULL
};


const wasm_valkind_t *ngx_wavm_arity_i64[] = {
    &ngx_wavm_i64,
    NULL
};


const wasm_valkind_t *ngx_wavm_arity_i32x2[] = {
    &ngx_wavm_i32, &ngx_wavm_i32,
    NULL
};


const wasm_valkind_t *ngx_wavm_arity_i32x3[] = {
    &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32,
    NULL
};


const wasm_valkind_t *ngx_wavm_arity_i32x4[] = {
    &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32,
    NULL
};


const wasm_valkind_t *ngx_wavm_arity_i32x5[] = {
    &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32,
    &ngx_wavm_i32,
    NULL
};


const wasm_valkind_t *ngx_wavm_arity_i32x6[] = {
    &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32,
    &ngx_wavm_i32, &ngx_wavm_i32,
    NULL
};


const wasm_valkind_t *ngx_wavm_arity_i32x8[] = {
    &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32,
    &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32,
    NULL
};


const wasm_valkind_t *ngx_wavm_arity_i32x9[] = {
    &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32,
    &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32,
    &ngx_wavm_i32,
    NULL
};


const wasm_valkind_t *ngx_wavm_arity_i32x10[] = {
    &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32,
    &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32,
    &ngx_wavm_i32, &ngx_wavm_i32,
    NULL
};


const wasm_valkind_t *ngx_wavm_arity_i32x12[] = {
    &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32,
    &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32,
    &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32, &ngx_wavm_i32,
    NULL
};


const wasm_valkind_t *ngx_wavm_arity_i32_i64[] = {
    &ngx_wavm_i32, &ngx_wavm_i64,
    NULL
};


const wasm_valkind_t *ngx_wavm_arity_i32_i64_i32[] = {
    &ngx_wavm_i32, &ngx_wavm_i64, &ngx_wavm_i32,
    NULL
};


static void
ngx_wavm_host_kindvec2typevec(const wasm_valkind_t **valkinds,
    wasm_valtype_vec_t *out)
{
    size_t           i;
    wasm_valkind_t  *valkind;

    if (valkinds == NULL) {
        wasm_valtype_vec_new_empty(out);
        return;
    }

    for (i = 0; ((wasm_valkind_t **) valkinds)[i]; i++) { /* void */ }

    wasm_valtype_vec_new_uninitialized(out, i);

    for (i = 0; i < out->size; i++) {
        valkind = ((wasm_valkind_t **) valkinds)[i];
        ngx_wasm_assert(valkind != NULL);
        out->data[i] = wasm_valtype_new(*valkind);
    }
}


ngx_wavm_hfunc_t *
ngx_wavm_host_hfunc_create(ngx_pool_t *pool, ngx_wavm_host_def_t *host,
    ngx_str_t *name)
{
    ngx_wavm_hfunc_t          *hfunc = NULL;
    ngx_wavm_host_func_def_t  *func;
    wasm_valtype_vec_t         args, rets;

    for (func = host->funcs; func->ptr; func++) {

        if (name->len != func->name.len) {
            continue;
        }

#if 0
        dd("strncmp: \"%.*s\", \"%.*s\", (len: %d)",
           (int) name->len, name->data,
           (int) func->name.len, func->name.data,
           (int) name->len);
#endif

        if (!ngx_str_eq(name->data, name->len, func->name.data, func->name.len)) {
            continue;
        }

        ngx_wavm_host_kindvec2typevec(func->args, &args);
        ngx_wavm_host_kindvec2typevec(func->rets, &rets);

        hfunc = ngx_pcalloc(pool, sizeof(ngx_wavm_hfunc_t));
        if (hfunc == NULL) {
            break;
        }

        hfunc->pool = pool;
        hfunc->def = func;
        hfunc->functype = wasm_functype_new(&args, &rets);
        break;
    }

    return hfunc;
}


void
ngx_wavm_host_hfunc_destroy(ngx_wavm_hfunc_t *hfunc)
{
    wasm_functype_delete(hfunc->functype);
    ngx_pfree(hfunc->pool, hfunc);
}


wasm_trap_t *
ngx_wavm_hfunc_trampoline(void *env,
#ifdef NGX_WASM_HAVE_WASMTIME
    wasmtime_caller_t *caller,
    const wasmtime_val_t *args, size_t nargs,
    wasmtime_val_t *rets, size_t nrets
#elif NGX_WASM_HAVE_WASMER
    const wasm_val_vec_t *args,
    wasm_val_vec_t *rets
#elif NGX_WASM_HAVE_V8
    const wasm_val_t args[],
    wasm_val_t rets[]
#endif
    )
{
    size_t                   errlen = 0, len;
    const char              *err = NULL;
    u_char                  *p, *buf, trapbuf[NGX_WAVM_HFUNCS_MAX_TRAP_LEN];
    ngx_int_t                rc;
    ngx_wavm_instance_t     *instance;
    wasm_val_t              *hargs, *hrets;
    wasm_byte_vec_t          trapmsg;
    wasm_trap_t             *trap = NULL;
    ngx_wavm_hfunc_t        *hfunc;
#ifdef NGX_WASM_HAVE_WASMTIME
    wasm_val_vec_t           vargs, vrets;

#elif NGX_WASM_HAVE_WASMER
    ngx_wasmer_hfunc_ctx_t  *hctx;

#elif NGX_WASM_HAVE_V8
    ngx_v8_hfunc_ctx_t      *hctx;

#else
#   error NYI: trampoline host context retrieval
#endif

#ifdef NGX_WASM_HAVE_WASMTIME
    hfunc = (ngx_wavm_hfunc_t *) env;
    instance = (ngx_wavm_instance_t *) ngx_wrt.get_ctx(caller);

    wasm_val_vec_new_uninitialized(&vargs, nargs);
    wasm_val_vec_new_uninitialized(&vrets, nrets);

    ngx_wasmtime_valvec2wasm(&vargs, (wasmtime_val_t *) args, nargs);

    hargs = (wasm_val_t *) vargs.data;
    hrets = (wasm_val_t *) vrets.data;

#elif NGX_WASM_HAVE_WASMER
    hctx = (ngx_wasmer_hfunc_ctx_t *) env;
    instance = hctx->instance;
    hfunc = hctx->hfunc;

    hargs = (wasm_val_t *) args->data;
    hrets = (wasm_val_t *) rets->data;

#elif NGX_WASM_HAVE_V8
    hctx = (ngx_v8_hfunc_ctx_t *) env;
    instance = hctx->instance;
    hfunc = hctx->hfunc;

    hargs = (wasm_val_t *) args;
    hrets = (wasm_val_t *) rets;
#endif

    dd("wasm hfuncs trampoline (hfunc: \"%*s\", instance: %p)",
       (int) hfunc->def->name.len, hfunc->def->name.data, instance);

    ngx_str_null(&instance->trapmsg);

    instance->trapbuf = (u_char *) &trapbuf;

    rc = hfunc->def->ptr(instance, hargs, hrets);

#ifdef NGX_WASM_HAVE_WASMTIME
    ngx_wasm_valvec2wasmtime(rets, &vrets);

    wasm_val_vec_delete(&vargs);
    wasm_val_vec_delete(&vrets);
#endif

    dd("wasm hfuncs trampoline rc: %ld", rc);

    switch (rc) {

    case NGX_WAVM_OK:
        if (!instance->trapmsg.len) {
            return NULL;
        }

        dd("host trap");

        break;

    case NGX_WAVM_ERROR:
        err = "host internal error";
        break;

    case NGX_WAVM_BAD_USAGE:
        err = "bad host usage";
        break;

    case NGX_WAVM_BAD_ARG:
        err = "bad host argument";
        break;

    case NGX_WAVM_NYI:
        err = "host function not yet implemented";

        ngx_wasm_assert(!instance->trapmsg.len);

        ngx_wavm_instance_trap_printf(instance, "%V", &hfunc->def->name);
        break;

    default:
        err = "invalid host function rc";
        break;

    }

    len = instance->trapmsg.len;
    if (len) {
        if (err) {
            errlen = ngx_strlen(err);
            len += errlen + 2; /* ': ' */
        }

        wasm_byte_vec_new_uninitialized(&trapmsg, len);
        buf = (u_char *) trapmsg.data;
        p = buf;

        if (err) {
            p = ngx_copy(p, err, errlen);
            p = ngx_copy(p, (u_char *) ": ", 2);
        }

        p = ngx_snprintf(p, len - (p - buf), "%V", &instance->trapmsg);

        ngx_wasm_assert( ((size_t) (p - buf)) == len );

    } else {
        wasm_name_new(&trapmsg, ngx_strlen(err)
#if (NGX_WASM_HAVE_WASMTIME || NGX_WASM_HAVE_V8)
                      /* wasm_trap_new requires a null-terminated string */
                      + 1
#endif
                      , err);
    }

    trap = ngx_wrt.trap_new(&instance->wrt_store, &trapmsg);

    wasm_byte_vec_delete(&trapmsg);

    return trap;
}
