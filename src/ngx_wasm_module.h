/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef NGX_WASM_MODULE_H_INCLUDED_
#define NGX_WASM_MODULE_H_INCLUDED_


#include <ngx_wasm.h>


#define NGX_WASM_MODULE            0x5741534d   /* "WASM" */
#define NGX_WASM_CONF              0x00300000


#define NGX_WASM_NO_VM_ACTIONS                                               \
    { NULL, NULL, NULL, NULL, NULL, NULL }


typedef struct {
    ngx_str_t                    name;
    void                        *(*create_conf)(ngx_cycle_t *cycle);
    char                        *(*init_conf)(ngx_cycle_t *cycle, void *conf);
    ngx_int_t                    (*init)(ngx_cycle_t *cycle);
    void                         (*exit_process)(ngx_cycle_t *cycle);
    void                         (*exit_master)(ngx_cycle_t *cycle);
    ngx_wasm_vm_actions_t        vm_actions;
} ngx_wasm_module_t;


ngx_wasm_vm_pt ngx_wasm_core_get_default_vm(ngx_cycle_t *cycle);


#endif /* NGX_WASM_MODULE_H_INCLUDED_ */


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
