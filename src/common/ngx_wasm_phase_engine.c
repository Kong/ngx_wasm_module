/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_phase_engine.h>


/* helpers */


static ngx_uint_t
ngx_wasm_phase_engine_nphases(ngx_uint_t module_type)
{
    switch (module_type) {

    case NGX_HTTP_MODULE:
          return NGX_HTTP_LOG_PHASE + 1;

    default:
          ngx_wasm_assert(0);
          break;

    }

    return NGX_CONF_UNSET_UINT;
}


static const char *
ngx_wasm_phase_engine_isolation_name(ngx_uint_t mode)
{
    switch (mode) {

    case NGX_WASM_PHASE_ENGINE_ISOLATION_MAIN:
        return "main";

    case NGX_WASM_PHASE_ENGINE_ISOLATION_SRV:
        return "server";

    case NGX_WASM_HTTP_MODULE_ISOLATION_LOC:
        return "location";

    case NGX_WASM_HTTP_MODULE_ISOLATION_REQ:
        return "request";

#if 0
    case NGX_WASM_STREAM_MODULE_ISOLATION_SESS:
        return "session";
#endif

    case NGX_WASM_PHASE_ENGINE_ISOLATION_EPH:
        return "ephemeral";

    default:
        ngx_wasm_assert(0);
        break;

    }

    return NULL;
}


static ngx_uint_t
ngx_wasm_phase_engine_isolation_mode(ngx_str_t *name, ngx_uint_t module_type)
{
#   define ngx_wasm_phase_engine_cmpmode(s, mode)                             \
        if (ngx_wasm_phase_engine_isolation_name(mode) &&                     \
            ngx_strncmp(s->data, ngx_wasm_phase_engine_isolation_name(mode),  \
                        s->len) == 0) return mode

    ngx_wasm_phase_engine_cmpmode(name, NGX_WASM_PHASE_ENGINE_ISOLATION_MAIN);
    ngx_wasm_phase_engine_cmpmode(name, NGX_WASM_PHASE_ENGINE_ISOLATION_SRV);
    ngx_wasm_phase_engine_cmpmode(name, NGX_WASM_PHASE_ENGINE_ISOLATION_EPH);

    switch (module_type) {

    case NGX_HTTP_MODULE:
        ngx_wasm_phase_engine_cmpmode(name, NGX_WASM_HTTP_MODULE_ISOLATION_LOC);
        ngx_wasm_phase_engine_cmpmode(name, NGX_WASM_HTTP_MODULE_ISOLATION_REQ);
        break;

#if 0
    case NGX_STREAM_MODULE:
        ngx_wasm_phase_engine_cmpmode(name,
                                     NGX_WASM_STREAM_MODULE_ISOLATION_EPH);
        break;
#endif

    default:
        ngx_wasm_assert(0);
        break;

    }

    return NGX_CONF_UNSET_UINT;

#   undef ngx_wasm_phase_engine_cmpmode
}


static char *
ngx_wasm_phase_engine_conf_parse_phase(ngx_conf_t *cf, u_char *name,
    ngx_uint_t *phase, ngx_uint_t module_type)
{
    size_t             i;
    ngx_uint_t         nphases;
    ngx_str_t         *phases;
    static ngx_str_t   http_phases[] = {
        ngx_string("post_read"),
        ngx_string("server_rewrite"),
        ngx_string("find_config"),
        ngx_string("rewrite"),
        ngx_string("post_rewrite"),
        ngx_string("pre_access"),
        ngx_string("access"),
        ngx_string("post_access"),
        ngx_string("pre_content"),
        ngx_string("content"),
        ngx_string("log")
    };

    nphases = ngx_wasm_phase_engine_nphases(module_type);

    switch (module_type) {

    case NGX_HTTP_MODULE:
        phases = http_phases;
        break;

    default:
        ngx_conf_log_error(NGX_LOG_ALERT, cf, 0, "unreachable");
        ngx_wasm_assert(0);
        return NGX_CONF_ERROR;

    }

    for (i = 0; i < nphases; i++) {
        if (ngx_strncmp(name, phases[i].data, phases[i].len) == 0) {
            *phase = i;
            return NGX_CONF_OK;
        }
    }

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "unknown phase \"%s\"",
                       name);

    return NGX_CONF_ERROR;
}


/* configuration & init */


void *
ngx_wasm_phase_engine_create_main_conf_annx(ngx_conf_t *cf)
{
    ngx_wasm_vm_t                          *vm;
    ngx_wasm_phase_engine_main_conf_annx_t  *mcf;

    vm = ngx_wasm_core_get_vm(cf->cycle);
    if (vm == NULL) {
        return NULL;
    }

    mcf = ngx_pcalloc(cf->pool, sizeof(ngx_wasm_phase_engine_main_conf_annx_t));
    if (mcf == NULL) {
        return NULL;
    }

    mcf->vmcache.vm = vm;

    ngx_wasm_vm_cache_init(&mcf->vmcache);

    return mcf;
}


void *
ngx_wasm_phase_engine_create_leaf_conf_annx(ngx_conf_t *cf)
{
    ngx_wasm_vm_t                          *vm;
    ngx_wasm_phase_engine_leaf_conf_annx_t  *lcf;

    vm = ngx_wasm_core_get_vm(cf->cycle);
    if (vm == NULL) {
        return NULL;
    }

    lcf = ngx_pcalloc(cf->pool, sizeof(ngx_wasm_phase_engine_leaf_conf_annx_t));
    if (lcf == NULL) {
        return NULL;
    }

    lcf->vm = vm;
    lcf->vmcache.vm = vm;
    lcf->module_type = NGX_CONF_UNSET_UINT;
    lcf->module_nphases = NGX_CONF_UNSET_UINT;
    lcf->isolation = NGX_CONF_UNSET_UINT;
    lcf->phase_engines = NULL;

    ngx_wasm_vm_cache_init(&lcf->vmcache);

    return lcf;
}


static char *
ngx_wasm_phase_engine_init_leaf_conf_annx(ngx_conf_t *cf,
    ngx_wasm_phase_engine_leaf_conf_annx_t *lcf, ngx_uint_t module_type)
{
    if (lcf->phase_engines == NULL) {
        lcf->module_type = module_type;
        lcf->module_nphases = ngx_wasm_phase_engine_nphases(module_type);

        lcf->phase_engines = ngx_pcalloc(cf->pool, lcf->module_nphases
                               * sizeof(ngx_wasm_phase_engine_phase_engine_t));
        if (lcf->phase_engines == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


char *
ngx_wasm_phase_engine_isolation_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_uint_t                              module_type = cmd->offset;
    ngx_str_t                              *value;
    ngx_wasm_phase_engine_leaf_conf_annx_t  *lcf = conf;

    value = cf->args->elts;

    if (ngx_wasm_phase_engine_init_leaf_conf_annx(cf, lcf, module_type)
        != NGX_CONF_OK)
    {
        return NGX_CONF_ERROR;
    }

    lcf->isolation = ngx_wasm_phase_engine_isolation_mode(&value[1], module_type);

    if (lcf->isolation == NGX_CONF_UNSET_UINT) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid isolation mode \"%V\"", &value[1]);
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


char *
ngx_wasm_phase_engine_call_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                               *value, phase_name;
    ngx_uint_t                               phase;
    ngx_uint_t                               module_type = cmd->offset;
    ngx_conf_post_t                         *post;
    ngx_wasm_phase_engine_leaf_conf_annx_t   *lcf = conf;
    ngx_wasm_phase_engine_call_t            **pcall, *call;
    ngx_wasm_phase_engine_phase_engine_t     *phase_engine;

    value = cf->args->elts;

    if (ngx_wasm_phase_engine_init_leaf_conf_annx(cf, lcf, module_type)
        != NGX_CONF_OK)
    {
        return NGX_CONF_ERROR;
    }

    call = ngx_pcalloc(cf->pool, sizeof(ngx_wasm_phase_engine_call_t));
    if (call == NULL) {
        return NGX_CONF_ERROR;
    }

    phase_name.data = value[1].data;
    phase_name.len = value[1].len;

    call->mod_name.data = value[2].data;
    call->mod_name.len = value[2].len;

    call->func_name.data = value[3].data;
    call->func_name.len = value[3].len;

    if (phase_name.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid phase \"%V\"",
                           &phase_name);
        return NGX_CONF_ERROR;
    }

    if (call->mod_name.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid module name \"%V\"",
                           &call->mod_name);
        return NGX_CONF_ERROR;
    }

    if (call->func_name.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid function name \"%V\"",
                           &call->func_name);
        return NGX_CONF_ERROR;
    }

    if (ngx_wasm_phase_engine_conf_parse_phase(cf, phase_name.data, &phase,
                                              module_type)
        != NGX_CONF_OK)
    {
        return NGX_CONF_ERROR;
    }

    if (ngx_wasm_vm_get_module(lcf->vm, &call->mod_name) == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] no \"%V\" module defined", &call->mod_name);
        return NGX_CONF_ERROR;
    }

    phase_engine = lcf->phase_engines[phase];

    if (phase_engine == NULL) {
        phase_engine = ngx_pcalloc(cf->pool,
                                   sizeof(ngx_wasm_phase_engine_phase_engine_t));
        if (phase_engine == NULL) {
            return NGX_CONF_ERROR;
        }

        phase_engine->phase = phase;
        phase_engine->phase_name.data = phase_name.data;
        phase_engine->phase_name.len = phase_name.len;
        phase_engine->calls = ngx_array_create(cf->pool, 2,
                                  sizeof(ngx_wasm_phase_engine_call_t *));
        if (phase_engine->calls == NULL) {
            return NGX_CONF_ERROR;
        }

        lcf->phase_engines[phase] = phase_engine;
    }

    pcall = ngx_array_push(phase_engine->calls);
    if (pcall == NULL) {
        return NGX_CONF_ERROR;
    }

    *pcall = call;

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, phase_engine);
    }

    return NGX_CONF_OK;
}


char *
ngx_wasm_phase_engine_merge_leaf_conf_annx(ngx_conf_t *cf, void *parent,
    void *child)
{
    size_t                                  i;
    ngx_wasm_phase_engine_leaf_conf_annx_t  *prev = parent;
    ngx_wasm_phase_engine_leaf_conf_annx_t  *conf = child;

    ngx_conf_merge_uint_value(conf->module_nphases, prev->module_nphases, 0);
    ngx_conf_merge_uint_value(conf->module_type, prev->module_type, 0);
    ngx_conf_merge_uint_value(conf->isolation, prev->isolation,
                              NGX_WASM_PHASE_ENGINE_ISOLATION_MAIN);

    ngx_conf_merge_ptr_value(conf->vm, prev->vm, NULL);

    if (conf->phase_engines == NULL) {
        conf->phase_engines = prev->phase_engines;
    }

    if (prev->phase_engines) {
        for (i = 0; i < conf->module_nphases; i++) {
            if (conf->phase_engines[i] == NULL) {
                conf->phase_engines[i] = prev->phase_engines[i];
            }
        }
    }

    return NGX_CONF_OK;
}


/* phases */


static void
ngx_wasm_phase_engine_rctx_pool_cleanup(void *data)
{
    ngx_wasm_vm_cache_t  *vmcache = data;

    ngx_wasm_vm_cache_free(vmcache);
}


static ngx_wasm_vm_cache_t *
ngx_wasm_phase_engine_vmcache(ngx_wasm_phase_engine_phase_ctx_t *pctx)
{
    ngx_wasm_vm_cache_t                     *vmcache = NULL;
    ngx_wasm_phase_engine_rctx_t            *rctx;
    ngx_wasm_phase_engine_leaf_conf_annx_t  *lcf;

    lcf = pctx->lcf;

    switch (lcf->isolation) {

    case NGX_WASM_PHASE_ENGINE_ISOLATION_MAIN:
        vmcache = &pctx->mcf->vmcache;
        break;

    case NGX_WASM_PHASE_ENGINE_ISOLATION_SRV:
        vmcache = &pctx->srv->vmcache;
        break;

    case NGX_WASM_HTTP_MODULE_ISOLATION_LOC:
        vmcache = &pctx->lcf->vmcache;
        break;

    case NGX_WASM_HTTP_MODULE_ISOLATION_REQ:
        vmcache = ngx_wasm_vm_cache_new(lcf->vm);
        if (vmcache == NULL) {
            ngx_log_error(NGX_LOG_EMERG, pctx->log, 0,
                          "failed to create vm cache (isolation: \"%s\")",
                          ngx_wasm_phase_engine_isolation_name(lcf->isolation));
            return NULL;
        }

        rctx = pctx->rctx;

        rctx->cln = ngx_pool_cleanup_add(rctx->pool, 0);
        if (rctx->cln == NULL) {
            ngx_wasm_vm_cache_free(vmcache);
            return NULL;
        }

        rctx->cln->handler = ngx_wasm_phase_engine_rctx_pool_cleanup;
        rctx->cln->data = vmcache;

        break;

    case NGX_WASM_PHASE_ENGINE_ISOLATION_EPH:
        /* void */
        break;

    default:
        ngx_log_error(NGX_LOG_ALERT, pctx->log, 0,
                      "NYI - wasm phase engine: unsupported "
                      "isolation mode \"%u\"", lcf->isolation);
        ngx_wasm_assert(0);
        break;

    }

#if (NGX_DEBUG)
    ngx_log_debug1(NGX_LOG_DEBUG_WASM, pctx->log, 0,
                   "wasm using \"%s\" isolation mode",
                   ngx_wasm_phase_engine_isolation_name(lcf->isolation));
#endif

    return vmcache;
}


ngx_int_t
ngx_wasm_phase_engine_exec(ngx_wasm_phase_engine_phase_ctx_t *pctx)
{
    size_t                                  i;
    ngx_int_t                               rc;
    ngx_uint_t                              phase;
    ngx_wasm_hctx_t                         hctx;
    ngx_wasm_vm_instance_t                 *inst;
    ngx_wasm_phase_engine_phase_engine_t    *phase_engine;
    ngx_wasm_phase_engine_leaf_conf_annx_t  *lcf;
    ngx_wasm_phase_engine_rctx_t            *rctx;
    ngx_wasm_phase_engine_call_t            *call;

    phase = pctx->phase;
    rctx = pctx->rctx;
    lcf = pctx->lcf;

    if (lcf->phase_engines == NULL) {
        return NGX_DECLINED;
    }

    phase_engine = lcf->phase_engines[phase];
    if (phase_engine == NULL) {
        return NGX_DECLINED;
    }

    if (rctx->vmcache == NULL) {
        rctx->vmcache = ngx_wasm_phase_engine_vmcache(pctx);
    }

    for (i = 0; i < phase_engine->calls->nelts; i++) {
        call = ((ngx_wasm_phase_engine_call_t **) phase_engine->calls->elts)[i];

        ngx_log_debug3(NGX_LOG_DEBUG_WASM, pctx->log, 0,
                       "wasm calling \"%V.%V\" in \"%V\" phase",
                       &call->mod_name, &call->func_name,
                       &phase_engine->phase_name);

        if (rctx->vmcache) {
            inst = ngx_wasm_vm_cache_get_instance(rctx->vmcache,
                                                  &call->mod_name);

        } else {
            inst = ngx_wasm_vm_instance_new(lcf->vm, &call->mod_name);
        }

        if (inst == NULL) {
            return NGX_ERROR;
        }

        hctx.log = pctx->log;
        hctx.pool = pctx->pool;
        hctx.data = pctx->data;

        ngx_wasm_vm_instance_set_hctx(inst, &hctx);

        rc = ngx_wasm_vm_instance_call(inst, &call->func_name);

        if (rctx->vmcache == NULL) {
            ngx_wasm_vm_instance_free(inst);
        }

        if (rc != NGX_OK) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}
