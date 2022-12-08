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
    ngx_wasm_core_conf_t  *wcf = conf;

    value = cf->args->elts;
    name = &value[1];
    path = &value[2];

    if (name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] invalid module name \"%V\"", name);
        return NGX_CONF_ERROR;
    }

    if (path->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "[wasm] invalid module path \"%V\"", path);
        return NGX_CONF_ERROR;
    }

    rc = ngx_wavm_module_add(wcf->vm, name, path);
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
