/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_PHASE_ENGINE_H_INCLUDED_
#define _NGX_WASM_PHASE_ENGINE_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_wasm_vm.h>
#include <ngx_wasm_vm_cache.h>


typedef enum {
    NGX_WASM_PHASE_ENGINE_ISOLATION_MAIN = 0,
    NGX_WASM_PHASE_ENGINE_ISOLATION_SRV,

    NGX_WASM_HTTP_MODULE_ISOLATION_LOC,
    NGX_WASM_HTTP_MODULE_ISOLATION_REQ,
    /*NGX_WASM_HTTP_MODULE_ISOLATION_CONN,*/

    /*NGX_WASM_STREAM_MODULE_ISOLATION_SESS,*/

    NGX_WASM_PHASE_ENGINE_ISOLATION_EPH
} ngx_wasm_phase_engine_isolation_mode_t;


typedef struct {
    ngx_str_t                                mod_name;
    ngx_str_t                                func_name;
} ngx_wasm_phase_engine_call_t;


typedef struct {
    ngx_uint_t                               phase;
    ngx_str_t                                phase_name;

    ngx_array_t                             *calls;
} ngx_wasm_phase_engine_phase_engine_t;


typedef struct {
    ngx_uint_t                               module_nphases;
    ngx_uint_t                               module_type;
    ngx_uint_t                               isolation;
    ngx_wasm_vm_cache_t                      vmcache;
    ngx_wasm_vm_t                           *vm;
    ngx_wasm_phase_engine_phase_engine_t   **phase_engines;
} ngx_wasm_phase_engine_leaf_conf_annx_t;


typedef struct {
    ngx_wasm_vm_cache_t                      vmcache;
} ngx_wasm_phase_engine_main_conf_annx_t;


typedef struct {
    ngx_pool_t                              *pool;
    ngx_pool_cleanup_t                      *cln;
    ngx_wasm_vm_cache_t                     *vmcache;
} ngx_wasm_phase_engine_rctx_t;


typedef struct {
    ngx_uint_t                               phase;
    ngx_pool_t                              *pool;
    ngx_log_t                               *log;
    ngx_wasm_phase_engine_main_conf_annx_t  *mcf;
    ngx_wasm_phase_engine_leaf_conf_annx_t  *srv;
    ngx_wasm_phase_engine_leaf_conf_annx_t  *lcf;
    ngx_wasm_phase_engine_rctx_t            *rctx;
    void                                    *data;
} ngx_wasm_phase_engine_phase_ctx_t;


void *ngx_wasm_phase_engine_create_main_conf_annx(ngx_conf_t *cf);

void *ngx_wasm_phase_engine_create_leaf_conf_annx(ngx_conf_t *cf);

char *ngx_wasm_phase_engine_isolation_directive(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

char *ngx_wasm_phase_engine_call_directive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

char *ngx_wasm_phase_engine_merge_leaf_conf_annx(ngx_conf_t *cf, void *parent,
    void *child);

ngx_int_t ngx_wasm_phase_engine_exec(ngx_wasm_phase_engine_phase_ctx_t *pctx);


#endif /* _NGX_WASM_PHASE_ENGINE_H_INCLUDED_ */
