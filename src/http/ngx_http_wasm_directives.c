#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>
#include <ngx_http_proxy_wasm.h>


#define NGX_WASM_DEFAULT_RESOLVER 1


char *
ngx_http_wasm_call_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                  *values, *phase_name, *module_name, *func_name;
    ngx_wasm_op_t              *op;
    ngx_http_wasm_loc_conf_t   *loc = conf;
    ngx_wasm_phase_t           *phase = ngx_http_wasm_subsystem.phases;
    ngx_wavm_t                 *vm;

    vm = ngx_wasm_main_vm(cf->cycle);
    if (vm == NULL) {
        return NGX_WASM_CONF_ERR_NO_WASM;
    }

    /* args */

    values = cf->args->elts;

    phase_name = &values[1];
    module_name = &values[2];
    func_name = &values[3];

    if (phase_name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid phase \"%V\"",
                           phase_name);
        return NGX_CONF_ERROR;
    }

    if (func_name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid function name \"%V\"",
                           func_name);
        return NGX_CONF_ERROR;
    }

    if (module_name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid module name \"%V\"",
                           module_name);
        return NGX_CONF_ERROR;
    }

    for (/* void */; phase->name.len; phase++) {
        if (ngx_str_eq(phase_name->data, phase_name->len,
                       phase->name.data, phase->name.len))
        {
            break;
        }
    }

    if (phase->name.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "unknown phase \"%V\"",
                           phase_name);
        return NGX_CONF_ERROR;
    }

    if (phase->on == 0) {
        ngx_conf_log_error(NGX_LOG_WASM_NYI, cf, 0, "unsupported phase \"%V\"",
                           phase_name);
        return NGX_CONF_ERROR;
    }

    /* op */

    op = ngx_pcalloc(cf->pool, sizeof(ngx_wasm_op_t));
    if (op == NULL) {
        return NGX_CONF_ERROR;
    }

    op->module = ngx_wavm_module_lookup(vm, module_name);
    if (op->module == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "no \"%V\" module defined", module_name);
        return NGX_CONF_ERROR;
    }

    op->host = &ngx_http_wasm_host_interface;
    op->code = NGX_WASM_OP_CALL;
    op->on_phases = phase->on;

    if (ngx_wasm_ops_plan_add(loc->plan, &op, 1) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    op->conf.call.func_name = *func_name;

    return NGX_CONF_OK;
}


char *
ngx_http_wasm_proxy_wasm_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_int_t                   rc;
    ngx_str_t                  *values, *name, *config;
    ngx_http_wasm_loc_conf_t   *loc = conf;
    ngx_http_wasm_main_conf_t  *mcf;

    mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_wasm_module);
    if (mcf->vm == NULL) {
        return NGX_WASM_CONF_ERR_NO_WASM;
    }

    /* args */

    values = cf->args->elts;

    name = &values[1];
    config = cf->args->nelts > 2 ? &values[2] : NULL;

    if (name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid module name \"%V\"", name);
        return NGX_CONF_ERROR;
    }

    rc = ngx_http_wasm_ops_add_filter(loc->plan, name, config,
                                      &mcf->store, mcf->vm);
    if (rc != NGX_OK) {
        if (rc == NGX_ABORT) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "no \"%V\" module defined", name);
        }

        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


char *
ngx_http_wasm_proxy_wasm_isolation_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_str_t                  *values, *value;
    ngx_http_wasm_loc_conf_t   *loc = conf;
    ngx_http_wasm_main_conf_t  *mcf;

    mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_wasm_module);
    if (mcf->vm == NULL) {
        return NGX_WASM_CONF_ERR_NO_WASM;
    }

    /* args */

    values = cf->args->elts;
    value = &values[1];

    if (value->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid isolation mode \"%V\"", value);
        return NGX_CONF_ERROR;
    }

    if (ngx_str_eq(value->data, value->len, "none", -1)) {
        loc->isolation = NGX_PROXY_WASM_ISOLATION_NONE;

    } else if (ngx_str_eq(value->data, value->len, "stream", -1)) {
        loc->isolation = NGX_PROXY_WASM_ISOLATION_STREAM;

    } else if (ngx_str_eq(value->data, value->len, "filter", -1)) {
        loc->isolation = NGX_PROXY_WASM_ISOLATION_FILTER;

    } else {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid isolation mode \"%V\"", value);
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


char *
ngx_http_wasm_resolver_add_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    in_addr_t                   addr;
    ngx_str_t                  *values, *host, *address;
    ngx_resolver_t             *r;
#if NGX_WASM_DEFAULT_RESOLVER
    ngx_resolver_t             *rm = NULL;
    ngx_resolver_node_t        *rnm;
    ngx_wasm_core_conf_t       *wcf;
#endif
    ngx_resolver_node_t        *rn;
    ngx_http_core_loc_conf_t   *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    r = clcf->resolver;

    if (r == NULL) {
#if !NGX_WASM_DEFAULT_RESOLVER
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "no resolver defined");
        return NGX_CONF_ERROR;
#else
        wcf = ngx_wasm_core_cycle_get_conf(cf->cycle);
        if (wcf == NULL) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "no resolver defined and no default resolver");
            return NGX_CONF_ERROR;
        }

        r = wcf->resolver;
#endif

    }
#if NGX_WASM_DEFAULT_RESOLVER
    else {
        wcf = ngx_wasm_core_cycle_get_conf(cf->cycle);
        if (wcf) {
            rm = wcf->resolver;
        }
    }
#endif

    /* args */

    values = cf->args->elts;
    address = &values[1];
    host = &values[2];

    if (address->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid address value \"%V\"", address);
        return NGX_CONF_ERROR;
    }

    if (host->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid host value \"%V\"", host);
        return NGX_CONF_ERROR;
    }

    rn = ngx_calloc(sizeof(ngx_resolver_node_t), cf->log);
    if (rn == NULL) {
        return NGX_CONF_ERROR;
    }

    rn->nlen = host->len;
    rn->name = ngx_alloc(rn->nlen, cf->log);
    if (rn->name == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memcpy(rn->name, host->data, rn->nlen);

    rn->ttl = NGX_MAX_UINT32_VALUE;
    rn->valid = NGX_MAX_UINT32_VALUE;
    rn->expire = NGX_MAX_UINT32_VALUE;
    rn->node.key = ngx_crc32_short(rn->name, rn->nlen);

    if (ngx_strlchr(address->data, address->data + address->len, ':')) {

#if (NGX_HAVE_INET6)
        if (!r->ipv6
            || ngx_inet6_addr(address->data, address->len,
                              rn->u6.addr6.s6_addr)
            != NGX_OK)
        {
#endif

            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid address value \"%V\"", address);
            return NGX_CONF_ERROR;

#if (NGX_HAVE_INET6)
        }

        rn->naddrs6 = 1;
#endif

    } else {
        addr = ngx_inet_addr(address->data, address->len);
        if (addr == INADDR_NONE) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid address value \"%V\"", address);
            return NGX_CONF_ERROR;
        }

        rn->naddrs = 1;
        rn->u.addr = addr;
    }

    ngx_rbtree_insert(&r->name_rbtree, &rn->node);

    ngx_queue_insert_head(&r->name_expire_queue, &rn->queue);

#if NGX_WASM_DEFAULT_RESOLVER
    if (rm) {
        /* insert in default resolver */
        rnm = ngx_alloc(sizeof(ngx_resolver_node_t), cf->log);
        if (rnm == NULL) {
            return NGX_CONF_ERROR;
        }

        ngx_memcpy(rnm, rn, sizeof(ngx_resolver_node_t));

        rnm->nlen = host->len;
        rnm->name = ngx_alloc(rnm->nlen, cf->log);
        if (rnm->name == NULL) {
            return NGX_CONF_ERROR;
        }

        ngx_memcpy(rnm->name, host->data, rnm->nlen);

        ngx_rbtree_insert(&rm->name_rbtree, &rnm->node);

        ngx_queue_insert_head(&rm->name_expire_queue, &rnm->queue);
    }
#endif

    return NGX_CONF_OK;
}
