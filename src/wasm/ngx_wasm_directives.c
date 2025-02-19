#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>
#include <ngx_wavm.h>


static char *
ngx_wasm_core_runtime_block(ngx_conf_t *cf, ngx_uint_t cmd_type)
{
    char        *rv;
    ngx_conf_t   save = *cf;

    cf->cmd_type = cmd_type;
    cf->module_type = NGX_WASM_MODULE;

    rv = ngx_conf_parse(cf, NULL);

    *cf = save;

    return rv;
}


char *
ngx_wasm_core_wasmtime_block(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{
    return ngx_wasm_core_runtime_block(cf, NGX_WASMTIME_CONF);
}


char *
ngx_wasm_core_wasmer_block(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{
    return ngx_wasm_core_runtime_block(cf, NGX_WASMER_CONF);
}


char *
ngx_wasm_core_v8_block(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{
    return ngx_wasm_core_runtime_block(cf, NGX_V8_CONF);
}


char *
ngx_wasm_core_metrics_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char           *rv;
    ngx_conf_t      save = *cf;
    ngx_wa_conf_t  *wacf = (ngx_wa_conf_t *) cf->ctx;

    if (wacf->metrics->config.initialized) {
        return NGX_WA_CONF_ERR_DUPLICATE;
    }

    wacf->metrics->config.initialized = 1;

    cf->cmd_type = NGX_METRICS_CONF;
    cf->module_type = NGX_WASM_MODULE;

    rv = ngx_conf_parse(cf, NULL);

    *cf = save;

    return rv;
}


static ngx_int_t
ngx_wasm_core_current_runtime_flag(ngx_conf_t *cf)
{
    if (cf->cmd_type == NGX_WASMTIME_CONF
        && !ngx_str_eq(NGX_WASM_RUNTIME, -1, "wasmtime", -1))
    {
        return NGX_DECLINED;
    }

    if (cf->cmd_type == NGX_WASMER_CONF
        && !ngx_str_eq(NGX_WASM_RUNTIME, -1, "wasmer", -1))
    {
        return NGX_DECLINED;
    }

    if (cf->cmd_type == NGX_V8_CONF
        && !ngx_str_eq(NGX_WASM_RUNTIME, -1, "v8", -1))
    {
        return NGX_DECLINED;
    }

    return NGX_OK;
}


char *
ngx_wasm_core_flag_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_int_t              rc;
    ngx_str_t             *value, *fname, *fval;
    ngx_wasm_core_conf_t  *wcf = conf;

    value = cf->args->elts;
    fname = &value[1];
    fval = &value[2];

    if (ngx_wasm_core_current_runtime_flag(cf) != NGX_OK) {
        /* flag from a different runtime block, ignoring it */
        return NGX_CONF_OK;
    }

    rc = ngx_wrt.conf_flags_add(&wcf->vm_conf.flags, fname, fval);
    switch (rc) {
    case NGX_OK:
        return NGX_CONF_OK;
    case NGX_DECLINED:
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] unsupported \"%s\" configuration flag: "
                           "\"%V\"", NGX_WASM_RUNTIME, fname);
        return NGX_CONF_ERROR;
    case NGX_ABORT:
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] unknown \"%s\" configuration flag: \"%V\"",
                           NGX_WASM_RUNTIME, fname);
        return NGX_CONF_ERROR;
    default:
        ngx_wa_assert(0);
        break;
    }

    return NGX_CONF_ERROR;
}


static ngx_int_t
validate_shm_size(ngx_conf_t *cf, ssize_t size, ngx_str_t *value)
{
    if (size == NGX_ERROR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] invalid shm size \"%V\"", value);
        return NGX_ERROR;
    }

    if (size < (ssize_t) NGX_WA_SHM_MIN_SIZE) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] shm size of %z bytes is too small, "
                           "minimum required is %z bytes",
                           size, NGX_WA_SHM_MIN_SIZE);
        return NGX_ERROR;
    }

    if ((size & (ngx_pagesize - 1)) != 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] shm size of %z bytes is not page-aligned, "
                           "must be a multiple of %z", size, ngx_pagesize);
        return NGX_ERROR;
    }

    return NGX_OK;
}


static char *
ngx_wasm_core_shm_generic_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf, ngx_wa_shm_type_e type)
{
    size_t                  i;
    ssize_t                 size;
    ngx_str_t              *value, *name, *arg3;
    ngx_array_t            *shms = ngx_wasmx_shms(cf->cycle);
    ngx_wa_shm_mapping_t   *mapping;
    ngx_wa_shm_t           *shm;
    ngx_wa_shm_eviction_e   eviction;

    value = cf->args->elts;
    name = &value[1];
    size = ngx_parse_size(&value[2]);
    arg3 = (cf->args->nelts == 4) ? &value[3] : NULL;
    eviction = NGX_WA_SHM_EVICTION_SLRU;

    if (!name->len) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] invalid shm name \"%V\"", name);
        return NGX_CONF_ERROR;
    }

    if (validate_shm_size(cf, size, &value[2]) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    if (arg3) {
        if (ngx_str_eq(arg3->data, arg3->len, "eviction=lru", -1)) {
            eviction = NGX_WA_SHM_EVICTION_LRU;

        } else if (ngx_str_eq(arg3->data, arg3->len, "eviction=slru", -1)) {
            eviction = NGX_WA_SHM_EVICTION_SLRU;

        } else if (ngx_str_eq(arg3->data, arg3->len, "eviction=none", -1)) {
            eviction = NGX_WA_SHM_EVICTION_NONE;

        } else if (ngx_strncmp(arg3->data, "eviction=", 9) == 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "[wasm] invalid eviction policy \"%s\"",
                               arg3->data + 9);
            return NGX_CONF_ERROR;

        } else {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "[wasm] invalid option \"%V\"",
                               arg3);
            return NGX_CONF_ERROR;
        }
    }

    shm = ngx_pcalloc(cf->pool, sizeof(ngx_wa_shm_t));
    if (shm == NULL) {
        return NGX_CONF_ERROR;
    }

    shm->type = type;
    shm->eviction = eviction;
    shm->name = *name;
    shm->log = cf->cycle->log;

    mapping = shms->elts;

    for (i = 0; i < shms->nelts; i++) {
        if (ngx_str_eq(mapping[i].name.data, mapping[i].name.len,
                       name->data, name->len))
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "[wasm] \"%V\" shm already defined", name);
            return NGX_CONF_ERROR;
        }
    }

    mapping = ngx_array_push(shms);
    if (mapping == NULL) {
        return NGX_CONF_ERROR;
    }

    mapping->name = *name;
    mapping->zone = ngx_shared_memory_add(cf, name, size, &ngx_wasmx_module);
    if (mapping->zone == NULL) {
        return NGX_CONF_ERROR;
    }

    mapping->zone->init = ngx_wa_shm_init_zone;
    mapping->zone->data = shm;
    mapping->zone->noreuse = 1;  /* TODO: enable shm reuse (fix SIGHUP) */

    return NGX_CONF_OK;
}


char *
ngx_wasm_core_module_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    size_t                 i;
    ngx_int_t              rc;
    ngx_str_t             *value, *name, *path;
    ngx_str_t             *config = NULL;
    ngx_wasm_core_conf_t  *wcf = conf;

    value = cf->args->elts;
    name = &value[1];
    path = &value[2];

    if (!name->len) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] invalid module name \"%V\"", name);
        return NGX_CONF_ERROR;
    }

    for (i = 0; i < name->len; i++) {
        if (name->data[i] == ' '
            || name->data[i] == '\t'
            || name->data[i] == ':')
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "[wasm] invalid module name \"%V\":"
                               " character not allowed \"%c\"",
                               name, name->data[i]);
            return NGX_CONF_ERROR;
        }
    }

    if (!path->len) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] invalid module path \"%V\"", path);
        return NGX_CONF_ERROR;
    }

    if (cf->args->nelts == 4) {
        config = &value[3];
    }

    rc = ngx_wavm_module_add(wcf->vm, name, path, config);
    if (rc != NGX_OK) {
        if (rc == NGX_DECLINED) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "[wasm] \"%V\" module already defined", name);
        }

        /* NGX_ERROR, NGX_ABORT */

        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


char *
ngx_wasm_core_shm_kv_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    return ngx_wasm_core_shm_generic_directive(cf, cmd,
                                               conf, NGX_WA_SHM_TYPE_KV);
}


char *
ngx_wasm_core_shm_queue_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_str_t  *args, *arg3;

    if (cf->args->nelts == 4) {
        args = cf->args->elts;
        arg3 = &args[3];

        if (ngx_strncmp(arg3->data, "eviction=", 9) == 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "[wasm] shm_queue \"%V\": queues do not "
                               "support eviction policies",
                               &args[1]);
            return NGX_CONF_ERROR;
        }
    }

    return ngx_wasm_core_shm_generic_directive(cf, cmd,
                                               conf, NGX_WA_SHM_TYPE_QUEUE);
}


char *
ngx_wasm_core_metrics_slab_size_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ssize_t            size;
    ngx_str_t         *value;
    ngx_wa_metrics_t  *metrics = ngx_wasmx_metrics(cf->cycle);

    if (metrics->config.slab_size != NGX_CONF_UNSET_SIZE) {
        return NGX_WA_CONF_ERR_DUPLICATE;
    }

    value = cf->args->elts;
    size = ngx_parse_size(&value[1]);

    if (validate_shm_size(cf, size, &value[1]) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    metrics->config.slab_size = size;

    return NGX_CONF_OK;
}


char *
ngx_wasm_core_metrics_max_metric_name_length_directive(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_int_t          n;
    ngx_str_t         *value;
    ngx_wa_metrics_t  *metrics = ngx_wasmx_metrics(cf->cycle);

    if (metrics->config.max_metric_name_length != NGX_CONF_UNSET_SIZE) {
        return NGX_WA_CONF_ERR_DUPLICATE;
    }

    value = cf->args->elts;
    n = ngx_atoi(value[1].data, value[1].len);
    if (n == NGX_ERROR) {
        /* ngx_conf_set_num_slot */
        return "invalid value";
    }

    if (n < 6) {
        /* "pw.%V.%V" */
        return "value too small (min: 6)";
    }

    metrics->config.max_metric_name_length = n;

    return NGX_CONF_OK;
}


char *
ngx_wasm_core_resolver_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_wasm_core_conf_t  *wcf = conf;
    ngx_str_t             *value;

    if (wcf->user_resolver) {
        return NGX_WA_CONF_ERR_DUPLICATE;
    }

    value = cf->args->elts;

    wcf->user_resolver = ngx_resolver_create(cf, &value[1],
                                             cf->args->nelts - 1);
    if (wcf->user_resolver == NULL) {
        return NGX_CONF_ERROR;
    }

    /* wcf->resolver will be freed by pool cleanup */
    wcf->resolver = wcf->user_resolver;

    return NGX_CONF_OK;
}


char *
ngx_wasm_core_pwm_lua_resolver_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
#if (NGX_WASM_LUA)
    return ngx_conf_set_flag_slot(cf, cmd, conf);
#else
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "[wasm] proxy_wasm_lua_resolver requires lua support");
    return NGX_CONF_ERROR;
#endif
}
