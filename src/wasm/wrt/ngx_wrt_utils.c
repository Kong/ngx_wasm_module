#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>
#include <ngx_wrt.h>


static ngx_wrt_flag_handler_t *
ngx_wrt_get_flag_handler(ngx_str_t *flag_name)
{
    size_t      i;
    ngx_str_t  *n;

    for (i = 0; ngx_wrt_flag_handlers[i].flag_name.len; i++) {
        n = &ngx_wrt_flag_handlers[i].flag_name;
        if (ngx_str_eq(flag_name->data, flag_name->len, n->data, n->len)) {
            return &ngx_wrt_flag_handlers[i];
        }
    }

    return NULL;
}


ngx_int_t
ngx_wrt_add_flag(ngx_array_t *flags, ngx_str_t *name, ngx_str_t *value)
{
    ngx_wrt_flag_t          *flag;
    ngx_wrt_flag_handler_t  *flag_handler = ngx_wrt_get_flag_handler(name);

    if (flag_handler && flag_handler->handler) {
        flag = ngx_array_push(flags);
        flag->name = *name;
        flag->value = *value;
        return NGX_OK;

    } else if (flag_handler) {
        return NGX_ABORT;

    } else {
        return NGX_DECLINED;
    }
}


void
ngx_wrt_apply_flags(wasm_config_t *config, ngx_wavm_conf_t *conf,
    ngx_log_t *log)
{
    size_t                   i;
    ngx_str_t               *name, *value;
    ngx_wrt_flag_t          *flag = conf->flags.elts;
    ngx_wrt_flag_handler_t  *handler = NULL;

    for (i = 0; i < conf->flags.nelts; i++) {
        name = &flag[i].name;
        value = &flag[i].value;

        ngx_wavm_log_error(NGX_LOG_INFO, log, NULL,
                           "setting flag \"%V=%V\"", name, value);

        handler = ngx_wrt_get_flag_handler(name);

        ngx_wasm_assert(handler);

        handler->handler(config, name, &flag[i].value, handler->wrt_config_set);
    }
}
