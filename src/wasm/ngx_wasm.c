#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>


static char *ngx_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_wasm_init(ngx_cycle_t *cycle);

ngx_uint_t  ngx_wasm_max_module;


static ngx_command_t  ngx_wasm_cmds[] = {

    { ngx_string("wasm"),
      NGX_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_wasm_block,
      0,
      0,
      NULL },

    ngx_null_command
};


static ngx_core_module_t  ngx_wasm_module_ctx = {
    ngx_string("wasm"),
    NULL,
    NULL
};


ngx_module_t  ngx_wasm_module = {
    NGX_MODULE_V1,
    &ngx_wasm_module_ctx,              /* module context */
    ngx_wasm_cmds,                     /* module directives */
    NGX_CORE_MODULE,                   /* module type */
    NULL,                              /* init master */
    ngx_wasm_init,                     /* init module */
    NULL,                              /* init process */
    NULL,                              /* init thread */
    NULL,                              /* exit thread */
    NULL,                              /* exit process */
    NULL,                              /* exit master */
    NGX_MODULE_V1_PADDING
};


static char *
ngx_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char                 *rv;
    void               ***ctx;
    ngx_uint_t            i;
    ngx_conf_t            pcf;
    ngx_wasm_module_t    *m;

    if (*(void **) conf) {
        return NGX_WASM_CONF_ERR_DUPLICATE;
    }

    ngx_wasm_max_module = ngx_count_modules(cf->cycle, NGX_WASM_MODULE);

    ctx = ngx_pcalloc(cf->pool, sizeof(void *));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    *ctx = ngx_pcalloc(cf->pool, ngx_wasm_max_module * sizeof(void *));
    if (*ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    *(void **) conf = ctx;

    /* NGX_WASM_MODULES create_conf */

    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        m = cf->cycle->modules[i]->ctx;

        if (m->create_conf) {
            (*ctx)[cf->cycle->modules[i]->ctx_index] =
                                                     m->create_conf(cf->cycle);
            if ((*ctx)[cf->cycle->modules[i]->ctx_index] == NULL) {
                return NGX_CONF_ERROR;
            }
        }
    }

    /* parse the wasm{} block */

    pcf = *cf;

    cf->ctx = ctx;
    cf->module_type = NGX_WASM_MODULE;
    cf->cmd_type = NGX_WASM_CONF;

    rv = ngx_conf_parse(cf, NULL);

    *cf = pcf;

    if (rv != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }

    /* NGX_WASM_MODULES init_conf */

    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        m = cf->cycle->modules[i]->ctx;

        if (m->init_conf) {
            rv = m->init_conf(cf->cycle,
                              (*ctx)[cf->cycle->modules[i]->ctx_index]);
            if (rv != NGX_CONF_OK) {
                return rv;
            }
        }
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_wasm_init(ngx_cycle_t *cycle)
{
    size_t              i;
    ngx_wasm_module_t  *m;
    ngx_int_t           rc;

    /* NGX_WASM_MODULES init */

    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->type != NGX_WASM_MODULE) {
            continue;
        }

        m = cycle->modules[i]->ctx;

        if (m->init) {
            rc = m->init(cycle);
            if (rc != NGX_OK) {
                return rc;
            }
        }
    }

    return NGX_OK;
}
