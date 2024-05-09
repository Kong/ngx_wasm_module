#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>
#include <ngx_wasm_subsystem.h>
#ifdef NGX_HTTP_WASM
#include <ngx_http_wasm.h>
#endif

#include <ngx_wa_metrics.h>

#if (!NGX_DEBUG)
#   error ngx_wasm_debug_module included in a non-debug build
#endif


/**
 * ngx_wasm_debug_module
 */
static ngx_int_t
ngx_wasm_debug_init(ngx_cycle_t *cycle)
{
    size_t                   long_metric_name_len = NGX_MAX_ERROR_STR;
    uint32_t                 mid;
    ngx_str_t                metric_name;
    u_char                   buf[long_metric_name_len];

    static ngx_wasm_phase_t  ngx_wasm_debug_phases[] = {
        { ngx_string("a_phase"), 0, 0, 0 },
        { ngx_null_string, 0, 0, 0 }
    };

    static ngx_wasm_subsystem_t  ngx_wasm_debug_subsystem = {
        2,
        NGX_WASM_SUBSYS_HTTP, /* stub */
        ngx_wasm_debug_phases,
    };

    /* coverage */

    /**
     * ngx_wasm_phase_lookup NULL return branch is unreacheable in
     * normal ngx_wasmx_module usage.
     */
    ngx_wa_assert(
        ngx_wasm_phase_lookup(&ngx_wasm_debug_subsystem, 3) == NULL
    );

    metric_name.len = long_metric_name_len;
    metric_name.data = buf;

    /* invalid metric name length */
    ngx_wa_assert(
        ngx_wa_metrics_define(ngx_wasmx_metrics(cycle),
                              &metric_name,
                              NGX_WA_METRIC_COUNTER,
                              &mid) == NGX_ABORT
    );

    /* invalid metric type */
    ngx_wa_assert(
        ngx_wa_metrics_define(ngx_wasmx_metrics(cycle),
                              &metric_name,
                              100,
                              &mid) == NGX_ABORT
    );

    return NGX_OK;
}


static ngx_wasm_module_t  ngx_wasm_debug_module_ctx = {
    NULL,                                  /* create configuration */
    NULL,                                  /* init configuration */
    ngx_wasm_debug_init                    /* init module */
};


ngx_module_t  ngx_wasm_debug_module = {
    NGX_MODULE_V1,
    &ngx_wasm_debug_module_ctx,            /* module context */
    NULL,                                  /* module directives */
    NGX_WASM_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


/**
 * ngx_http_wasm_debug_filter_module
 */
#ifdef NGX_WASM_HTTP
typedef struct {
    ngx_int_t            header_rc;
    ngx_flag_t           header_rc_set;

    ngx_int_t            body_rc;
    ngx_flag_t           body_rc_set;
} ngx_http_wasm_debug_filter_loc_conf_t;


typedef struct {
    unsigned             entered_header_filter:1;
    unsigned             entered_body_filter:1;
} ngx_http_wasm_debug_filter_ctx_t;


static char *
ngx_conf_set_signed_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char             *p = conf;
    unsigned          neg = 0;
    u_char           *start;
    ngx_int_t        *np;
    ngx_str_t        *strval, *value;
    ngx_conf_post_t  *post;

    np = (ngx_int_t *) (p + cmd->offset);
    if (*np != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    strval = cf->args->elts;
    value = &strval[1];
    start = &value->data[0];

    if (value->len && *start == '-') {
        start++;
        value->len--;
        neg = 1;
    }

    *np = ngx_atoi(start, value->len);
    if (*np == NGX_ERROR) {
        return "invalid number";
    }

    if (neg) {
        *np = -*np;
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, cmd, conf);
    }

    return NGX_CONF_OK;
}


static char *
post_set_signed(ngx_conf_t *cf, void *data, void *conf)
{
    char           *p = conf;
    ngx_command_t  *cmd = data;
    ngx_flag_t     *fl;

    fl = (ngx_flag_t *) (p + cmd->offset + sizeof(ngx_int_t));

    *fl = 1;

    return NGX_CONF_OK;
}


static ngx_conf_post_t  ngx_conf_post_set_signed_slot = { post_set_signed };
static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
ngx_module_t  ngx_http_wasm_debug_filter_module;


static ngx_command_t  ngx_http_wasm_debug_filter_cmds[] = {

    { ngx_string("wasm_debug_header_filter_return"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_signed_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_wasm_debug_filter_loc_conf_t, header_rc),
      &ngx_conf_post_set_signed_slot },

    { ngx_string("wasm_debug_body_filter_return"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_signed_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_wasm_debug_filter_loc_conf_t, body_rc),
      &ngx_conf_post_set_signed_slot },

      ngx_null_command
};


static ngx_http_wasm_debug_filter_ctx_t *
ngx_http_wasm_debug_filter_get_ctx(ngx_http_request_t *r)
{
    ngx_http_wasm_debug_filter_ctx_t  *rctx;

    rctx = ngx_http_get_module_ctx(r, ngx_http_wasm_debug_filter_module);
    if (rctx == NULL) {
        rctx = ngx_pcalloc(r->pool, sizeof(ngx_http_wasm_debug_filter_ctx_t));
        if (rctx == NULL) {
            return NULL;
        }

        ngx_http_set_ctx(r, rctx, ngx_http_wasm_debug_filter_module);
    }

    return rctx;
}


static ngx_int_t
ngx_http_wasm_debug_header_filter_handler(ngx_http_request_t *r)
{
    ngx_int_t                               rc;
    ngx_http_wasm_debug_filter_ctx_t       *rctx;
    ngx_http_wasm_debug_filter_loc_conf_t  *loc;

    rctx = ngx_http_wasm_debug_filter_get_ctx(r);
    loc = ngx_http_get_module_loc_conf(r, ngx_http_wasm_debug_filter_module);

    dd("enter");

    if (!rctx->entered_header_filter) {
        rctx->entered_header_filter = 1;

        if (loc->header_rc_set) {
            rc = loc->header_rc;
            goto done;
        }
    }

    rc = ngx_http_next_header_filter(r);

done:

    dd("exit rc: %ld", rc);
    return rc;
}


static ngx_int_t
ngx_http_wasm_debug_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_wasm_debug_header_filter_handler;

    return NGX_OK;
}


static void *
ngx_http_wasm_debug_filter_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_wasm_debug_filter_loc_conf_t  *loc;

    loc = ngx_pcalloc(cf->pool, sizeof(ngx_http_wasm_debug_filter_loc_conf_t));
    if (loc == NULL) {
        return NULL;
    }

    loc->header_rc = NGX_CONF_UNSET;
    loc->header_rc_set = 0;

    loc->body_rc = NGX_CONF_UNSET;
    loc->body_rc_set = 0;

    return loc;
}


static ngx_http_module_t  ngx_http_wasm_debug_filter_module_ctx = {
    NULL,                                        /* preconfiguration */
    ngx_http_wasm_debug_filter_init,             /* postconfiguration */
    NULL,                                        /* create main configuration */
    NULL,                                        /* init main configuration */
    NULL,                                        /* create server configuration */
    NULL,                                        /* merge server configuration */
    ngx_http_wasm_debug_filter_create_loc_conf,  /* create location configuration */
    NULL                                         /* merge location configuration */
};


ngx_module_t  ngx_http_wasm_debug_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_wasm_debug_filter_module_ctx,      /* module context */
    ngx_http_wasm_debug_filter_cmds,             /* module directives */
    NGX_HTTP_MODULE,                             /* module type */
    NULL,                                        /* init master */
    NULL,                                        /* init module */
    NULL,                                        /* init process */
    NULL,                                        /* init thread */
    NULL,                                        /* exit thread */
    NULL,                                        /* exit process */
    NULL,                                        /* exit master */
    NGX_MODULE_V1_PADDING
};
#endif
