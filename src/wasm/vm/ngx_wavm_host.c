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

        if (ngx_strncmp(func->name.data, name->data, name->len) != 0) {
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
ngx_wavm_hfuncs_trampoline(void *env,
#if (NGX_WASM_HAVE_WASMTIME)
    const wasm_val_t args[], wasm_val_t rets[]
#else
    const wasm_val_vec_t* args, wasm_val_vec_t* rets
#endif
    )
{
    size_t                  errlen, len;
    char                   *err = NULL;
    u_char                 *p, *buf, trapbuf[NGX_WAVM_HFUNCS_MAX_TRAP_LEN];
    ngx_int_t               rc;
    ngx_wavm_hfunc_tctx_t  *tctx = env;
    ngx_wavm_instance_t    *instance = tctx->instance;
    ngx_wavm_hfunc_t       *hfunc = tctx->hfunc;
    wasm_val_t             *hargs, *hrets;
    wasm_byte_vec_t         trapmsg;
    wasm_trap_t            *trap = NULL;

#if (NGX_WASM_HAVE_WASMTIME)
    hargs = (wasm_val_t *) args;
    hrets = (wasm_val_t *) rets;

#else
    hargs = (wasm_val_t *) args->data;
    hrets = (wasm_val_t *) rets->data;
#endif

    ngx_log_debug2(NGX_LOG_DEBUG_WASM, instance->log, 0,
                   "wasm hfuncs trampoline (hfunc: \"%V\", tctx: %p)",
                   &hfunc->def->name, tctx);

    ngx_str_null(&instance->trapmsg);

    instance->trapbuf = (u_char *) &trapbuf;
    instance->mem_offset = (u_char *) wasm_memory_data(instance->memory);

    rc = hfunc->def->ptr(instance, hargs, hrets);

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, instance->log, 0,
                   "wasm hfuncs trampoline rc: \"%d\"", rc);

    switch (rc) {

    case NGX_WAVM_OK:
        break;

    case NGX_WAVM_ERROR:
        err = "nginx hfuncs error";
        goto trap;

    case NGX_WAVM_BAD_CTX:
        err = "bad context";
        goto trap;

    case NGX_WAVM_BAD_USAGE:
        err = "bad usage";
        goto trap;

    default:
        err = "NYI - hfuncs trampoline rc";
        goto trap;

    }

    return NULL;

trap:

    errlen = ngx_strlen(err);

    if (instance->trapmsg.len) {
        len = errlen + instance->trapmsg.len + 2;

        wasm_byte_vec_new_uninitialized(&trapmsg, len);

        buf = (u_char *) trapmsg.data;
        p = ngx_copy(buf, err, errlen);
        p = ngx_snprintf(p, len - (p - buf), ": %V", &instance->trapmsg);
        //*p++ = '\0'; /* trapmsg null terminated */

        ngx_wasm_assert( ((size_t) (p - buf)) == len );

    } else {
        wasm_byte_vec_new(&trapmsg, errlen, err);
    }

    trap = wasm_trap_new(instance->ctx->store, &trapmsg);
    wasm_byte_vec_delete(&trapmsg);

    return trap;
}


void
ngx_wavm_instance_trap_printf(ngx_wavm_instance_t *instance,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...)

#else
    const char *fmt, va_list args)
#endif
{
    u_char               *p;
    static const size_t   maxlen = NGX_WAVM_HFUNCS_MAX_TRAP_LEN - 1;

#if (NGX_HAVE_VARIADIC_MACROS)
    va_list  args;

    va_start(args, fmt);
    p = ngx_vsnprintf(instance->trapbuf, maxlen, fmt, args);
    va_end(args);

#else
    p = ngx_vsnprintf(instance->trapbuf, maxlen, fmt, args);
#endif

    *p++ = '\0';

    instance->trapmsg.len = p - instance->trapbuf;
    instance->trapmsg.data = (u_char *) instance->trapbuf;
}
