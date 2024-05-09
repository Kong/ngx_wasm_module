#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>
#ifdef NGX_WA_IPC
#include <ngx_ipc.h>
#endif


static char *ngx_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
#ifdef NGX_WA_IPC
static char *ngx_ipc_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
#endif
static ngx_int_t ngx_wasmx_init(ngx_cycle_t *cycle);


ngx_uint_t             ngx_wasm_max_module = 0;
ngx_uint_t             ngx_ipc_max_module = 0;


static ngx_command_t  ngx_wasmx_cmds[] = {

    { ngx_string("wasm"),
      NGX_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_wasm_block,
      0,
      0,
      NULL },

#ifdef NGX_WA_IPC
    { ngx_string("ipc"),
      NGX_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_ipc_block,
      0,
      0,
      NULL },
#endif

    ngx_null_command
};


static ngx_core_module_t  ngx_wasmx_module_ctx = {
    ngx_string("wasmx"),
    NULL,
    NULL
};


ngx_module_t  ngx_wasmx_module = {
    NGX_MODULE_V1,
    &ngx_wasmx_module_ctx,             /* module context */
    ngx_wasmx_cmds,                    /* module directives */
    NGX_CORE_MODULE,                   /* module type */
    NULL,                              /* init master */
    ngx_wasmx_init,                    /* init module */
    NULL,                              /* init process */
    NULL,                              /* init thread */
    NULL,                              /* exit thread */
    NULL,                              /* exit process */
    NULL,                              /* exit master */
    NGX_MODULE_V1_PADDING
};


static char *
ngx_wasmx_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf,
    ngx_uint_t type, ngx_uint_t conf_type)
{
    void                    **confs;
    char                     *rv;
    ngx_module_t             *m;
    ngx_wa_conf_t            *wacf = NULL;
    ngx_uint_t                i;
    ngx_conf_t                pcf;
    ngx_wa_create_conf_pt     create_conf;
    ngx_wa_init_conf_pt       init_conf;

    if (*(void **) conf) {
        /* ngx_wasmx_module conf found, wasm{} or ipc{} */

        wacf = *(ngx_wa_conf_t **) conf;

        if (wacf->initialized_types & conf_type) {
            return NGX_WA_CONF_ERR_DUPLICATE;
        }
    }

    if (wacf == NULL) {
        ngx_wasm_max_module = ngx_count_modules(cf->cycle, NGX_WASM_MODULE);
#ifdef NGX_WA_IPC
        ngx_ipc_max_module = ngx_count_modules(cf->cycle, NGX_IPC_MODULE);
#endif

        wacf = ngx_pcalloc(cf->pool, sizeof(ngx_wa_conf_t));
        if (wacf == NULL) {
            return NGX_CONF_ERROR;
        }

        wacf->wasm_confs = ngx_pcalloc(cf->pool,
                                       sizeof(void *) * ngx_wasm_max_module);
        if (wacf->wasm_confs == NULL) {
            return NGX_CONF_ERROR;
        }

#ifdef NGX_WA_IPC
        wacf->ipc_confs = ngx_pcalloc(cf->pool,
                                      sizeof(void *) * ngx_ipc_max_module);
        if (wacf->ipc_confs == NULL) {
            return NGX_CONF_ERROR;
        }
#endif

        wacf->metrics = ngx_wa_metrics_alloc(cf->cycle);
        if (wacf->metrics == NULL) {
            return NGX_CONF_ERROR;
        }

        *(ngx_wa_conf_t **) conf = wacf;
    }

    wacf->initialized_types |= conf_type;

    /* create_conf */

    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != type
            || !(wacf->initialized_types & conf_type))
        {
            continue;
        }

        m = cf->cycle->modules[i];
        create_conf = NULL;

        switch (type) {
        case NGX_WASM_MODULE: {
            ngx_wasm_module_t *wm = m->ctx;
            create_conf = wm->create_conf;
            confs = wacf->wasm_confs;
            break;
        }
#ifdef NGX_WA_IPC
        case NGX_IPC_MODULE: {
            ngx_ipc_module_t *im = m->ctx;
            create_conf = im->create_conf;
            confs = wacf->ipc_confs;
            break;
        }
#endif
        default:
            ngx_wa_assert(0);
            return NGX_CONF_ERROR;
        }

        if (create_conf) {
            confs[m->ctx_index] = create_conf(cf);
            if (confs[m->ctx_index] == NULL) {
                return NGX_CONF_ERROR;
            }
        }
    }

    /* parse the wasm{} block */

    pcf = *cf;

    cf->ctx = wacf;
    cf->module_type = type;
    cf->cmd_type = conf_type;

    rv = ngx_conf_parse(cf, NULL);

    *cf = pcf;

    if (rv != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }

    /* init_conf */

    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != type
            || !(wacf->initialized_types & conf_type))
        {
            continue;
        }

        m = cf->cycle->modules[i];
        init_conf = NULL;

        switch (type) {
        case NGX_WASM_MODULE: {
            ngx_wasm_module_t *wm = m->ctx;
            init_conf = wm->init_conf;
            confs = wacf->wasm_confs;
            break;
        }
#ifdef NGX_WA_IPC
        case NGX_IPC_MODULE: {
            ngx_ipc_module_t *im = m->ctx;
            init_conf = im->init_conf;
            confs = wacf->ipc_confs;
            break;
        }
#endif
        default:
            ngx_wa_assert(0);
            return NGX_CONF_ERROR;
        }

        if (init_conf) {
            rv = init_conf(cf, confs[m->ctx_index]);
            if (rv != NGX_CONF_OK) {
                return rv;
            }
        }
    }

    /* metrics init_conf */

    rv = ngx_wa_metrics_init_conf(wacf->metrics, cf);
    if (rv != NGX_CONF_OK) {
        return rv;
    }

    return NGX_CONF_OK;
}


static char *
ngx_wasm_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    return ngx_wasmx_block(cf, cmd, conf, NGX_WASM_MODULE, NGX_WASM_CONF);
}


#ifdef NGX_WA_IPC
static char *
ngx_ipc_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    return ngx_wasmx_block(cf, cmd, conf, NGX_IPC_MODULE, NGX_IPC_CONF);
}
#endif


static ngx_int_t
ngx_wasmx_init(ngx_cycle_t *cycle)
{
    size_t          i;
    ngx_int_t       rc;
    ngx_wa_conf_t  *wacf;

    wacf = ngx_wa_cycle_get_conf(cycle);
    if (wacf == NULL) {
        return NGX_OK;
    }

    rc = ngx_wa_metrics_init(wacf->metrics, cycle);
    if (rc != NGX_OK) {
        return rc;
    }

    /* NGX_WASM_MODULES + NGX_IPC_MODULES init */

    for (i = 0; cycle->modules[i]; i++) {
        rc = NGX_OK;

        switch (cycle->modules[i]->type) {
        case NGX_WASM_MODULE:
            if (wacf->initialized_types & NGX_WASM_CONF) {
                ngx_wasm_module_t *wm = cycle->modules[i]->ctx;
                if (wm->init) {
                    rc = wm->init(cycle);
                }
            }

            break;
#ifdef NGX_WA_IPC
        case NGX_IPC_MODULE:
            if (wacf->initialized_types & NGX_IPC_CONF) {
                ngx_ipc_module_t *im = cycle->modules[i]->ctx;
                if (im->init) {
                    rc = im->init(cycle);
                }
            }

            break;
#endif
        default:
            continue;
        }

        if (rc != NGX_OK) {
            return rc;
        }
    }

    return NGX_OK;
}


ngx_inline ngx_wa_metrics_t *
ngx_wasmx_metrics(ngx_cycle_t *cycle)
{
    ngx_wa_conf_t  *wacf;

    if (cycle->conf_ctx == NULL) {
        return NULL;
    }

    wacf = ngx_wa_cycle_get_conf(cycle);
    if (wacf == NULL) {
        return NULL;
    }

    return wacf->metrics;
}
