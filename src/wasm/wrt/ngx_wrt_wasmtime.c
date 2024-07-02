#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wrt.h>
#include <ngx_wavm_host.h>
#include <ngx_wasi.h>


#define NGX_WRT_WASMTIME_STACK_NARGS  10
#define NGX_WRT_WASMTIME_STACK_NRETS  10


typedef void (*wasmtime_config_set_int_pt)(wasm_config_t *config,
    uint64_t value);
typedef void (*wasmtime_config_set_bool_pt)(wasm_config_t *config, bool value);


static u_char *ngx_wasmtime_log_handler(ngx_wrt_res_t *res, u_char *buf,
    size_t len);


static ngx_int_t
size_flag_handler(wasm_config_t *config, ngx_str_t *name, ngx_str_t *value,
    ngx_log_t *log, void *wrt_setter)
{
    size_t                      v;
    wasmtime_config_set_int_pt  f = wrt_setter;

    v = ngx_parse_size(value);
    if (v == (size_t) NGX_ERROR) {
        ngx_log_error(NGX_LOG_EMERG, log, 0,
                      "failed setting wasmtime flag: "
                      "invalid value \"%V\"", value);
        return NGX_ERROR;
    }

    f(config, v);

    return NGX_OK;
}


static ngx_int_t
bool_flag_handler(wasm_config_t *config, ngx_str_t *name, ngx_str_t *value,
    ngx_log_t *log, void *wrt_setter)
{
    wasmtime_config_set_bool_pt  f = wrt_setter;

    if (ngx_str_eq(value->data, value->len, "on", -1)) {
        f(config, true);

    } else if (ngx_str_eq(value->data, value->len, "off", -1)) {
        f(config, false);

    } else {
        ngx_log_error(NGX_LOG_EMERG, log, 0,
                      "failed setting wasmtime flag: "
                      "invalid value \"%V\"", value);
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
strategy_flag_handler(wasm_config_t *config, ngx_str_t *name, ngx_str_t *value,
    ngx_log_t *log, void *wrt_setter)
{
    if (ngx_str_eq(value->data, value->len, "auto", -1)) {
        wasmtime_config_strategy_set(config, WASMTIME_STRATEGY_AUTO);

    } else if (ngx_str_eq(value->data, value->len, "cranelift", -1)) {
        wasmtime_config_strategy_set(config, WASMTIME_STRATEGY_CRANELIFT);
    }

    return NGX_OK;
}


static ngx_int_t
opt_level_flag_handler(wasm_config_t *config, ngx_str_t *name, ngx_str_t *value,
    ngx_log_t *log, void *wrt_setter)
{
    if (ngx_str_eq(value->data, value->len, "none", -1)) {
        wasmtime_config_cranelift_opt_level_set(config,
                                                WASMTIME_OPT_LEVEL_NONE);

    } else if (ngx_str_eq(value->data, value->len, "speed", -1)) {
        wasmtime_config_cranelift_opt_level_set(config,
                                                WASMTIME_OPT_LEVEL_SPEED);

    } else if (ngx_str_eq(value->data, value->len, "speed_and_size", -1)) {
        wasmtime_config_cranelift_opt_level_set(config,
            WASMTIME_OPT_LEVEL_SPEED_AND_SIZE);
    }

    return NGX_OK;
}


static ngx_int_t
profiler_flag_handler(wasm_config_t *config, ngx_str_t *name, ngx_str_t *value,
    ngx_log_t *log, void *wrt_setter)
{
    if (ngx_str_eq(value->data, value->len, "none", -1)) {
        wasmtime_config_profiler_set(config,
                                     WASMTIME_PROFILING_STRATEGY_NONE);

    } else if (ngx_str_eq(value->data, value->len, "jitdump", -1)) {
        wasmtime_config_profiler_set(config,
                                     WASMTIME_PROFILING_STRATEGY_JITDUMP);

    } else if (ngx_str_eq(value->data, value->len, "vtune", -1)) {
        wasmtime_config_profiler_set(config,
                                     WASMTIME_PROFILING_STRATEGY_VTUNE);

#if WASMTIME_VERSION_MAJOR >= 8

    } else if (ngx_str_eq(value->data, value->len, "perfmap", -1)) {
        wasmtime_config_profiler_set(config,
                                     WASMTIME_PROFILING_STRATEGY_PERFMAP);

#endif
    }

    return NGX_OK;
}


static wasm_config_t *
ngx_wasmtime_init_conf(ngx_wavm_conf_t *conf, ngx_log_t *log)
{
    char              *pathname;
    u_char            *errmsg;
    wasm_config_t     *config;
    wasmtime_error_t  *err;
#if 0
    wasm_name_t        msg;
#endif

    ngx_wa_assert(conf->backtraces != NGX_CONF_UNSET);

    if (conf->backtraces) {
        setenv("WASMTIME_BACKTRACE_DETAILS", "1", 1);
        setenv("RUST_BACKTRACE", "full", 1);

    } else {
        setenv("WASMTIME_BACKTRACE_DETAILS", "0", 0);
        setenv("RUST_BACKTRACE", "0", 0);
    }

    config = wasm_config_new();
    if (config == NULL) {
        return NULL;
    }

    wasmtime_config_wasm_reference_types_set(config, true);
    wasmtime_config_parallel_compilation_set(config, false);

#ifdef NGX_WASM_HAVE_NOPOOL
    wasmtime_config_debug_info_set(config, false);
    wasmtime_config_cranelift_opt_level_set(config, WASMTIME_OPT_LEVEL_NONE);
    wasmtime_config_static_memory_maximum_size_set(config, 0);
#endif

    if (conf->cache_config.len) {
        ngx_wavm_log_error(NGX_LOG_INFO, log, NULL,
                           "setting wasmtime cache config file: \"%V\"",
                           &conf->cache_config);

        pathname = ngx_calloc(conf->cache_config.len + 1, log);
        if (pathname == NULL) {
            goto error;
        }

        ngx_memcpy(pathname, conf->cache_config.data, conf->cache_config.len);

        err = wasmtime_config_cache_config_load(config, pathname);

        ngx_free(pathname);

        if (err) {
            errmsg = ngx_calloc(NGX_MAX_ERROR_STR + 1, log);
            if (errmsg == NULL) {
                goto error;
            }

            ngx_wasmtime_log_handler((ngx_wrt_res_t *) err, errmsg,
                                     NGX_MAX_ERROR_STR);

            ngx_log_error(NGX_LOG_EMERG, log, 0,
                          "failed configuring wasmtime cache; %s",
                          errmsg);

            goto error;
        }
    }

#if (NGX_DARWIN)
    wasmtime_config_macos_use_mach_ports_set(config, false);
#endif

    if (conf->compiler.len) {
        if (ngx_str_eq(conf->compiler.data, conf->compiler.len,
                       "auto", -1))
        {
            wasmtime_config_strategy_set(config, WASMTIME_STRATEGY_AUTO);

        } else if (ngx_str_eq(conf->compiler.data, conf->compiler.len,
                              "cranelift", -1))
        {
            wasmtime_config_strategy_set(config, WASMTIME_STRATEGY_CRANELIFT);

        } else {
            ngx_wavm_log_error(NGX_LOG_ERR, log, NULL,
                               "invalid compiler \"%V\"",
                               &conf->compiler);
            goto error;
        }
#if 0
        if (err) {
            wasmtime_error_message(err, &msg);

            ngx_wavm_log_error(NGX_LOG_ERR, log, NULL,
                               "failed setting \"%V\" compiler: %.*s",
                               &conf->compiler, msg.size, msg.data);

            wasmtime_error_delete(err);
            wasm_name_delete(&msg);

            goto error;
        }
#endif
    }

    if (ngx_wrt_apply_flags(config, conf, log) != NGX_OK) {
        goto error;
    }

    return config;

error:

    wasm_config_delete(config);

    return NULL;
}


static ngx_int_t
ngx_wasmtime_init_engine(ngx_wrt_engine_t *engine, wasm_config_t *config,
    ngx_pool_t *pool, ngx_wrt_err_t *err)
{
    size_t                     i, nwasi;
    ngx_wavm_host_func_def_t  *def;
    ngx_wavm_hfunc_t          *hfunc;
    static const char         *wasi_module = "wasi_snapshot_preview1";

    engine->engine = wasm_engine_new_with_config(config);
    if (engine->engine == NULL) {
        return NGX_ERROR;
    }

    engine->linker = wasmtime_linker_new(engine->engine);
    if (engine->linker == NULL) {
        return NGX_ERROR;
    }

    wasmtime_linker_allow_shadowing(engine->linker, 1);

    err->res = wasmtime_linker_define_wasi(engine->linker);
    if (err->res) {
        return NGX_ERROR;
    }

    /* load our own WASI implementations */

    nwasi = 0;

    for (i = 0; ngx_wasi_host.funcs[i].ptr; i++) {
        nwasi++;
    }

    engine->pool = pool;
    engine->wasi_hfuncs = ngx_pcalloc(pool, sizeof(ngx_wavm_hfunc_t *) * nwasi);

    for (i = 0; ngx_wasi_host.funcs[i].ptr; i++) {
        def = &ngx_wasi_host.funcs[i];

        hfunc = ngx_wavm_host_hfunc_create(pool, &ngx_wasi_host, &def->name);

        err->res = wasmtime_linker_define_func(engine->linker,
                                               wasi_module,
                                               ngx_strlen(wasi_module),
                                               (const char*) def->name.data,
                                               def->name.len,
                                               hfunc->functype,
                                               ngx_wavm_hfunc_trampoline,
                                               hfunc, NULL);
        if (err->res) {
            return NGX_ERROR;
        }

        engine->wasi_hfuncs[i] = hfunc;
    }

    return NGX_OK;
}


static void
ngx_wasmtime_destroy_engine(ngx_wrt_engine_t *engine)
{
    size_t i;

    for (i = 0; ngx_wasi_host.funcs[i].ptr; i++) {
        wasm_functype_delete(engine->wasi_hfuncs[i]->functype);
    }

    ngx_pfree(engine->pool, engine->wasi_hfuncs);

    wasmtime_linker_delete(engine->linker);
    wasm_engine_delete(engine->engine);
}


static ngx_int_t
ngx_wasmtime_wat2wasm(wasm_byte_vec_t *wat, wasm_byte_vec_t *wasm,
    ngx_wrt_err_t *err)
{
    err->res = wasmtime_wat2wasm(wat->data, wat->size, wasm);

    return err->res == NULL ? NGX_OK : NGX_ERROR;
}


static ngx_int_t
ngx_wasmtime_validate(ngx_wrt_engine_t *engine, wasm_byte_vec_t *bytes,
    ngx_wrt_err_t *err)
{
    err->res = wasmtime_module_validate(engine->engine, (u_char *) bytes->data,
                                        bytes->size);
    if (err->res) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_wasmtime_init_module(ngx_wrt_module_t *module, ngx_wrt_engine_t *engine,
    wasm_byte_vec_t *bytes, wasm_importtype_vec_t *imports,
    wasm_exporttype_vec_t *exports, ngx_wrt_err_t *err)
{
    err->res = wasmtime_module_new(engine->engine, (u_char *) bytes->data,
                                   bytes->size, &module->module);
    if (err->res) {
        return NGX_ERROR;
    }

    wasmtime_module_imports(module->module, imports);
    wasmtime_module_exports(module->module, exports);

    module->import_types = imports;
    module->export_types = exports;
    module->engine = engine;

    return NGX_OK;
}


static ngx_int_t
ngx_wasmtime_link_module(ngx_wrt_module_t *module, ngx_array_t *hfuncs,
    ngx_wrt_err_t *err)
{
    size_t                    i;
    const wasm_importtype_t  *importtype;
    const wasm_name_t        *importmodule, *importname;
    ngx_wavm_hfunc_t         *hfunc;

    for (i = 0; i < hfuncs->nelts; i++) {
        hfunc = ((ngx_wavm_hfunc_t **) hfuncs->elts)[i];

        importtype = ((wasm_importtype_t **)
                      module->import_types->data)[hfunc->idx];
        importmodule = wasm_importtype_module(importtype);
        importname = wasm_importtype_name(importtype);

        dd("  \"%.*s.%.*s\" import (%lu/%lu) ",
           (int) importmodule->size, importmodule->data,
           (int) importname->size, importname->data,
           hfunc->idx + 1, module->import_types->size);

        if (!ngx_str_eq(importmodule->data, importmodule->size, "env", -1)) {
            dd("  skipping");
            continue;
        }

        dd("   -> \"%.*s\" hfunc",
           (int) hfunc->def->name.len, hfunc->def->name.data);

        err->res = wasmtime_linker_define_func(module->engine->linker,
                                               importmodule->data,
                                               importmodule->size,
                                               importname->data,
                                               importname->size,
                                               hfunc->functype,
                                               ngx_wavm_hfunc_trampoline,
                                               hfunc, NULL);
        if (err->res) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


static void
ngx_wasmtime_destroy_module(ngx_wrt_module_t *module)
{
    wasmtime_module_delete(module->module);
}


static ngx_int_t
ngx_wasmtime_init_store(ngx_wrt_store_t *store, ngx_wrt_engine_t *engine,
    void *data)
{
    store->store = wasmtime_store_new(engine->engine, NULL, NULL);
    if (store->store == NULL) {
        return NGX_ERROR;
    }

    store->context = wasmtime_store_context(store->store);

    wasmtime_context_set_data(store->context, data);

    return NGX_OK;
}


static void
ngx_wasmtime_destroy_store(ngx_wrt_store_t *store)
{
    wasmtime_store_delete(store->store);
}


static ngx_int_t
ngx_wasmtime_init_instance(ngx_wrt_instance_t *instance, ngx_wrt_store_t *store,
    ngx_wrt_module_t *module, ngx_pool_t *pool, ngx_wrt_err_t *err)
{
    instance->pool = pool;
    instance->store = store;
    instance->module = module;
    instance->wasi_config = wasi_config_new();

#if 0
    wasi_config_inherit_argv(instance->wasi_config);
    wasi_config_inherit_env(instance->wasi_config);
    wasi_config_inherit_stdin(instance->wasi_config);
    wasi_config_inherit_stdout(instance->wasi_config);
    wasi_config_inherit_stderr(instance->wasi_config);
#endif

    err->res = wasmtime_context_set_wasi(store->context,
                                         instance->wasi_config);
    if (err->res) {
        return NGX_ERROR;
    }

    err->res = wasmtime_linker_instantiate(module->engine->linker,
                                           store->context,
                                           module->module,
                                           &instance->instance,
                                           &err->trap);
    if (err->res) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static void
ngx_wasmtime_destroy_instance(ngx_wrt_instance_t *instance)
{
    /* NOP */
    return;
}


static ngx_int_t
ngx_wasmtime_init_extern(ngx_wrt_extern_t *ext, ngx_wrt_instance_t *instance,
    ngx_uint_t idx)
{
    ngx_wrt_store_t           *s = instance->store;
    ngx_wrt_module_t          *m = instance->module;
    wasm_exporttype_t         *exporttype;
    const wasm_externtype_t   *externtype;
    char                      *name;
    size_t                     name_len;

    if (!wasmtime_instance_export_nth(s->context, &instance->instance, idx,
                                      &name, &name_len, &ext->ext))
    {
        return NGX_ERROR;
    }

    ngx_wa_assert(idx < m->export_types->size);

    ext->instance = instance;
    ext->context = s->context;
    ext->name.len = name_len;
    ext->name.data = ngx_pnalloc(instance->pool, name_len);
    if (ext->name.data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(ext->name.data, name, name_len);

    exporttype = m->export_types->data[idx];
    externtype = wasm_exporttype_type(exporttype);

    switch (wasm_externtype_kind(externtype)) {

    case WASM_EXTERN_FUNC:
        ngx_wa_assert(ext->ext.kind == WASMTIME_EXTERN_FUNC);
        ext->kind = NGX_WRT_EXTERN_FUNC;
        break;

    case WASM_EXTERN_MEMORY:
        ngx_wa_assert(ext->ext.kind == WASMTIME_EXTERN_MEMORY);
        ext->kind = NGX_WRT_EXTERN_MEMORY;
        instance->memory = &ext->ext.of.memory;
        break;

    case WASM_EXTERN_GLOBAL:
    case WASM_EXTERN_TABLE:
        break;

    default:
        /* NYI */
        ngx_wa_assert(0);
        return NGX_ABORT;

    }

    return NGX_OK;
}


static void
ngx_wasmtime_destroy_extern(ngx_wrt_extern_t *ext)
{
    ngx_pfree(ext->instance->pool, ext->name.data);
}


void
ngx_wasm_valvec2wasmtime(wasmtime_val_t *out, wasm_val_vec_t *vec)
{
    size_t   i;

    for (i = 0; i < vec->size; i++) {

        switch (vec->data[i].kind) {

        case WASM_I32:
            out[i].kind = WASMTIME_I32;
            out[i].of.i32 = vec->data[i].of.i32;
            break;

        case WASM_I64:
            out[i].kind = WASMTIME_I64;
            out[i].of.i64 = vec->data[i].of.i64;
            break;

        default:
            /* NYI */
            ngx_wa_assert(0);
            break;

        }
    }
}


void
ngx_wasmtime_valvec2wasm(wasm_val_vec_t *out, wasmtime_val_t *vec, size_t nvals)
{
    size_t           i;
    wasmtime_val_t  *val;

    ngx_wa_assert(out->size == nvals);

    for (i = 0; i < nvals; i++) {
        val = vec + i;

        switch (val->kind) {

        case WASMTIME_I32:
            out->data[i].kind = WASM_I32;
            out->data[i].of.i32 = vec[i].of.i32;
            break;

        case WASMTIME_I64:
            out->data[i].kind = WASM_I64;
            out->data[i].of.i64 = vec[i].of.i64;
            break;

        case WASMTIME_F32:
            out->data[i].kind = WASM_F32;
            out->data[i].of.f32 = vec[i].of.f32;
            break;

        case WASMTIME_F64:
            out->data[i].kind = WASM_F64;
            out->data[i].of.f64 = vec[i].of.f64;
            break;

        case WASMTIME_V128:
            /* NYI */
            ngx_wa_assert(0);
            break;

        case WASMTIME_FUNCREF:
            /* NYI */
            ngx_wa_assert(0);
            break;

        case WASMTIME_EXTERNREF:
            /* NYI */
            ngx_wa_assert(0);
            break;

        default:
            /* NYI */
            ngx_wa_assert(0);
            break;

        }
    }
}


static ngx_int_t
ngx_wasmtime_call(ngx_wrt_instance_t *instance, ngx_str_t *func_name,
    ngx_uint_t func_idx, wasm_val_vec_t *args, wasm_val_vec_t *rets,
    ngx_wrt_err_t *err)
{
    ngx_int_t           rc = NGX_ERROR;;
    wasmtime_extern_t   item;
    wasmtime_func_t    *func;
    wasmtime_val_t     *wargs = NULL, *wrets = NULL,
                        swargs[NGX_WRT_WASMTIME_STACK_NARGS],
                        swrets[NGX_WRT_WASMTIME_STACK_NRETS];

    if (!wasmtime_instance_export_get(instance->store->context,
                                      &instance->instance,
                                      (const char *) func_name->data,
                                      func_name->len, &item))
    {
        goto done;
    }

    ngx_wa_assert(item.kind == WASMTIME_EXTERN_FUNC);

    func = &item.of.func;

    if (args->size <= NGX_WRT_WASMTIME_STACK_NARGS) {
        wargs = &swargs[0];

    } else {
        wargs = ngx_pcalloc(instance->pool,
                            sizeof(wasmtime_val_t) * args->size);
        if (wargs == NULL) {
            goto done;
        }
    }

    ngx_wasm_valvec2wasmtime(wargs, args);

    if (rets->size <= NGX_WRT_WASMTIME_STACK_NRETS) {
        wrets = &swrets[0];

    } else {
        wrets = ngx_pcalloc(instance->pool,
                            sizeof(wasmtime_val_t) * rets->size);
        if (wrets == NULL) {
            goto done;
        }
    }

    err->res = wasmtime_func_call(instance->store->context,
                                  func,
                                  wargs, args->size,
                                  wrets, rets->size,
                                  &err->trap);
    if (err->trap || err->res) {
        rc = NGX_ABORT;
        goto done;
    }

    ngx_wasmtime_valvec2wasm(rets, wrets, rets->size);

    rc = NGX_OK;

done:

    if (args->size > NGX_WRT_WASMTIME_STACK_NARGS && wargs) {
        ngx_pfree(instance->pool, wargs);
    }

    if (rets->size > NGX_WRT_WASMTIME_STACK_NRETS && wrets) {
        ngx_pfree(instance->pool, wrets);
    }

    return rc;
}


static wasm_trap_t *
ngx_wasmtime_trap(ngx_wrt_store_t *store, wasm_byte_vec_t *msg)
{
    return wasmtime_trap_new((const char *) msg->data, msg->size);
}


static void *
ngx_wasmtime_get_ctx(void *data)
{
    wasmtime_caller_t   *caller = data;
    wasmtime_context_t  *context = wasmtime_caller_context(caller);

    return wasmtime_context_get_data(context);
}


static u_char *
ngx_wasmtime_log_handler(ngx_wrt_res_t *res, u_char *buf, size_t len)
{
    u_char            *p = buf;
    wasmtime_error_t  *error = res;
    wasm_message_t     errmsg;

    wasmtime_error_message(error, &errmsg);

    p = ngx_snprintf(buf, len, "%*s", errmsg.size, errmsg.data);

    wasm_byte_vec_delete(&errmsg);
    wasmtime_error_delete(error);

    return p;
}


static ngx_wrt_flag_handler_t  flag_handlers[] = {
    { ngx_string("debug_info"),
      bool_flag_handler,
      wasmtime_config_debug_info_set },

    { ngx_string("consume_fuel"),
      bool_flag_handler,
      wasmtime_config_consume_fuel_set },

    { ngx_string("epoch_interruption"),
      bool_flag_handler,
      wasmtime_config_epoch_interruption_set },

    { ngx_string("max_wasm_stack"),
      size_flag_handler,
      wasmtime_config_max_wasm_stack_set },

    { ngx_string("wasm_threads"),
      bool_flag_handler,
      wasmtime_config_wasm_threads_set },

    { ngx_string("wasm_reference_types"),
      bool_flag_handler,
      wasmtime_config_wasm_reference_types_set },

    { ngx_string("wasm_relaxed_simd"),
      bool_flag_handler,
      wasmtime_config_wasm_relaxed_simd_set },

    { ngx_string("wasm_simd"),
      bool_flag_handler,
      wasmtime_config_wasm_simd_set },

    { ngx_string("wasm_bulk_memory"),
      bool_flag_handler,
      wasmtime_config_wasm_bulk_memory_set },

    { ngx_string("wasm_multi_value"),
      bool_flag_handler,
      wasmtime_config_wasm_multi_value_set },

    { ngx_string("wasm_multi_memory"),
      bool_flag_handler,
      wasmtime_config_wasm_multi_memory_set },

    { ngx_string("wasm_memory64"),
      bool_flag_handler,
      wasmtime_config_wasm_memory64_set },

    { ngx_string("strategy"),
      strategy_flag_handler,
      NULL }, /* wasmtime_strategy_t */

    { ngx_string("parallel_compilation"),
      bool_flag_handler,
      wasmtime_config_parallel_compilation_set },

    { ngx_string("cranelift_debug_verifier"),
      bool_flag_handler,
      wasmtime_config_cranelift_debug_verifier_set },

    { ngx_string("cranelift_nan_canonicalization"),
      bool_flag_handler,
      wasmtime_config_cranelift_nan_canonicalization_set },

    { ngx_string("cranelift_opt_level"),
      opt_level_flag_handler,
      NULL }, /* wasmtime_opt_level_t */

    { ngx_string("profiler"),
      profiler_flag_handler,
      NULL }, /* wasmtime_profiling_strategy_t */

    { ngx_string("static_memory_maximum_size"),
      size_flag_handler,
      wasmtime_config_static_memory_maximum_size_set },

    { ngx_string("static_memory_guard_size"),
      size_flag_handler,
      wasmtime_config_static_memory_guard_size_set },

    { ngx_string("dynamic_memory_guard_size"),
      size_flag_handler,
      wasmtime_config_dynamic_memory_guard_size_set },

    { ngx_null_string, NULL, NULL },
};


ngx_wrt_t  ngx_wrt = {
    flag_handlers,
    ngx_wasmtime_init_conf,
    ngx_wrt_add_flag,
    ngx_wasmtime_init_engine,
    ngx_wasmtime_destroy_engine,
    ngx_wasmtime_validate,
    ngx_wasmtime_wat2wasm,
    ngx_wasmtime_init_module,
    ngx_wasmtime_link_module,
    ngx_wasmtime_destroy_module,
    ngx_wasmtime_init_store,
    ngx_wasmtime_destroy_store,
    ngx_wasmtime_init_instance,
    ngx_wasmtime_destroy_instance,
    ngx_wasmtime_call,
    ngx_wasmtime_init_extern,
    ngx_wasmtime_destroy_extern,
    ngx_wasmtime_trap,
    ngx_wasmtime_get_ctx,
    ngx_wasmtime_log_handler,
};
