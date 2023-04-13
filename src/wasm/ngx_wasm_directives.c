#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>
#include <ngx_wavm.h>


static char *
ngx_wasm_core_shm_generic_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf, ngx_wasm_shm_type_e type)
{
    size_t                   i;
    ssize_t                  size;
    ngx_str_t               *value, *name;
    ngx_wasm_core_conf_t    *wcf = conf;
    ngx_wasm_shm_mapping_t  *mapping;
    ngx_wasm_shm_t          *shm;
    const ssize_t            min_size = 3 * ngx_pagesize;

    value = cf->args->elts;
    name = &value[1];
    size = ngx_parse_size(&value[2]);

    if (!name->len) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] invalid shm name \"%V\"", name);
        return NGX_CONF_ERROR;
    }

    if (size == NGX_ERROR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] invalid shm size \"%V\"", &value[2]);
        return NGX_CONF_ERROR;
    }

    if (size < min_size) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] shm size of %d bytes is too small, "
                           "minimum required is %d bytes", size, min_size);
        return NGX_CONF_ERROR;
    }

    if ((size & (ngx_pagesize - 1)) != 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] shm size of %d bytes is not page-aligned, "
                           "must be a multiple of %d", size, ngx_pagesize);
        return NGX_CONF_ERROR;
    }

    shm = ngx_pcalloc(cf->pool, sizeof(ngx_wasm_shm_t));
    if (shm == NULL) {
        return NGX_CONF_ERROR;
    }

    shm->type = type;
    shm->name = *name;
    shm->log = cf->cycle->log;

    mapping = wcf->shms.elts;

    for (i = 0; i < wcf->shms.nelts; i++) {
        if (ngx_str_eq(mapping[i].name.data, mapping[i].name.len,
                       name->data, name->len))
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "[wasm] \"%V\" shm already defined", name);
            return NGX_CONF_ERROR;
        }
    }

    mapping = ngx_array_push(&wcf->shms);
    if (mapping == NULL) {
        return NGX_CONF_ERROR;
    }

    mapping->name = *name;
    mapping->zone = ngx_shared_memory_add(cf, name, size, &ngx_wasm_module);
    if (mapping->zone == NULL) {
        return NGX_CONF_ERROR;
    }

    mapping->zone->init = ngx_wasm_shm_init_zone;
    mapping->zone->data = shm;
    mapping->zone->noreuse = 1;  /* TODO: enable shm reuse (fix SIGHUP) */

    return NGX_CONF_OK;
}


char *
ngx_wasm_core_module_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
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
                                               conf, NGX_WASM_SHM_TYPE_KV);
}


char *
ngx_wasm_core_shm_queue_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    return ngx_wasm_core_shm_generic_directive(cf, cmd,
                                               conf, NGX_WASM_SHM_TYPE_QUEUE);
}


char *
ngx_wasm_core_resolver_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_wasm_core_conf_t  *wcf = conf;
    ngx_str_t             *value;

    if (wcf->user_resolver) {
        return "is duplicate";
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
    ngx_str_t             *value, *flag_name, *flag_value;
    ngx_wasm_core_conf_t  *wcf = conf;

    value = cf->args->elts;
    flag_name = &value[1];
    flag_value = &value[2];

    if (ngx_wasm_core_current_runtime_flag(cf) != NGX_OK) {
        /* flag from a different runtime block, ignoring it */
        return NGX_CONF_OK;
    }

    switch (ngx_wrt.conf_add_flag(&wcf->vm_conf.flags, flag_name, flag_value)) {

    case NGX_OK:
        return NGX_CONF_OK;

    case NGX_ABORT:
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] unsupported \"%s\" configuration flag: "
                           "\"%V\"",
                           NGX_WASM_RUNTIME, flag_name);

        return NGX_CONF_ERROR;

    case NGX_DECLINED:
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] unknown \"%s\" configuration flag: \"%V\"",
                           NGX_WASM_RUNTIME, flag_name);

        return NGX_CONF_ERROR;

    default:
        ngx_wasm_assert(0);

        return NGX_CONF_ERROR;
    }

}


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
};


char *
ngx_wasm_core_wasmtime_block(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{
    return ngx_wasm_core_runtime_block(cf, NGX_WASMTIME_CONF);
};


char *
ngx_wasm_core_wasmer_block(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{
    return ngx_wasm_core_runtime_block(cf, NGX_WASMER_CONF);
};


char *
ngx_wasm_core_v8_block(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{
    return ngx_wasm_core_runtime_block(cf, NGX_V8_CONF);
};
