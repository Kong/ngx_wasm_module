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
    if (op == NGX_CONF_ERROR) {
        return NGX_CONF_ERROR;
    }

    op->module = ngx_wavm_module_lookup(loc->ops_engine->vm, module_name);
    if (op->module == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "no \"%V\" module defined", module_name);
        return NGX_CONF_ERROR;
    }

    op->code = NGX_WASM_OP_CALL;
    op->on_phases = phase->on;
    op->host = &ngx_http_wasm_host_interface;

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
    u_char                    *p;
    ngx_str_t                 *values, *module_name, *filter_config;
    ngx_wasm_op_t             *op;
    ngx_proxy_wasm_t          *pwm;
    ngx_http_wasm_loc_conf_t  *loc = conf;

    if (loc->vm == NULL) {
        return NGX_WASM_CONF_ERR_NO_WASM;
    }

    /* args */

    values = cf->args->elts;

    module_name = &values[1];

    if (module_name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid module name \"%V\"", module_name);
        return NGX_CONF_ERROR;
    }

    /* filter alloc */

    pwm = ngx_pcalloc(cf->pool, sizeof(ngx_proxy_wasm_t));
    if (pwm == NULL) {
        return NGX_CONF_ERROR;
    }

    pwm->pool = cf->pool;
    pwm->log = &cf->cycle->new_log;

    if (cf->args->nelts > 2) {
        filter_config = &values[2];

        pwm->filter_config.len = filter_config->len;
        pwm->filter_config.data = ngx_palloc(pwm->pool,
                                             pwm->filter_config.len + 1);
        if (pwm->filter_config.data == NULL) {
            return NGX_CONF_ERROR;
        }

        p = ngx_copy(pwm->filter_config.data, filter_config->data,
                     pwm->filter_config.len);
        *p = '\0';
    }

    /* filter init */

    pwm->ecode_ = ngx_http_proxy_wasm_ecode;
    pwm->resume_ = ngx_http_proxy_wasm_resume;
    pwm->get_context_ = ngx_http_proxy_wasm_get_context;
    pwm->destroy_context_ = ngx_http_proxy_wasm_destroy_context;
    pwm->max_pairs = NGX_HTTP_WASM_MAX_REQ_HEADERS;

    pwm->module = ngx_wavm_module_lookup(loc->ops_engine->vm, module_name);
    if (pwm->module == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "no \"%V\" module defined", module_name);
        return NGX_CONF_ERROR;
    }

    /* op */

    op = ngx_pcalloc(cf->pool, sizeof(ngx_wasm_op_t));
    if (op == NULL) {
        return NGX_CONF_ERROR;
    }

    op->module = pwm->module;
    op->code = NGX_WASM_OP_PROXY_WASM;
    op->host = &ngx_proxy_wasm_host;
    op->on_phases = (1 << NGX_HTTP_REWRITE_PHASE)
                    | (1 << NGX_HTTP_ACCESS_PHASE)
                    | (1 << NGX_HTTP_CONTENT_PHASE)
                    | (1 << NGX_HTTP_WASM_HEADER_FILTER_PHASE)
                    | (1 << NGX_HTTP_WASM_BODY_FILTER_PHASE)
                    | (1 << NGX_HTTP_LOG_PHASE)
                    | (1 << NGX_HTTP_WASM_DONE_PHASE);

    if (ngx_wasm_ops_add(loc->ops_engine, op) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    op->conf.proxy_wasm.pwmodule = pwm;

    return NGX_CONF_OK;
}
