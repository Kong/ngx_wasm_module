#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>
#include <ngx_http_proxy_wasm.h>


char *
ngx_http_wasm_call_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                 *values, *phase_name, *module_name, *func_name;
    ngx_wasm_op_t             *op;
    ngx_http_wasm_loc_conf_t  *loc = conf;
    ngx_wasm_phase_t          *phase = ngx_http_wasm_subsystem.phases;

    if (loc->vm == NULL) {
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
        if (ngx_strncmp(phase_name->data, phase->name.data, phase->name.len)
            == 0)
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

    op->module = ngx_wavm_module_lookup(loc->ops_engine->vm, module_name);
    if (op->module == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "no \"%V\" module defined", module_name);
        return NGX_CONF_ERROR;
    }

    op->host = &ngx_http_wasm_host_interface;
    op->code = NGX_WASM_OP_CALL;
    op->on_phases = phase->on;

    if (ngx_wasm_ops_add(loc->ops_engine, op) != NGX_OK) {
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

    if (loc->vm == NULL) {
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

    rc = ngx_http_wasm_ops_add_filter(loc->ops_engine,
                                      cf->pool, &cf->cycle->new_log,
                                      name, config,
                                      &loc->isolation,
                                      &mcf->store);
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
    ngx_str_t                 *values, *value;
    ngx_http_wasm_loc_conf_t  *loc = conf;

    if (loc->vm == NULL) {
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

    if (ngx_strncmp(value->data, "none", 4) == 0) {
        loc->isolation = NGX_PROXY_WASM_ISOLATION_NONE;

    } else if (ngx_strncmp(value->data, "stream", 6) == 0) {
        loc->isolation = NGX_PROXY_WASM_ISOLATION_STREAM;

    } else if (ngx_strncmp(value->data, "filter", 6) == 0) {
        loc->isolation = NGX_PROXY_WASM_ISOLATION_FILTER;

    } else {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid isolation mode \"%V\"", value);
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}
