#ifndef _NGX_WASM_H_INCLUDED_
#define _NGX_WASM_H_INCLUDED_


#include <ngx_core.h>

#include <wasm.h>

#if (NGX_DEBUG)
#include <assert.h>
#   define ngx_wasm_assert(a)        assert(a)
#else
#   define ngx_wasm_assert(a)
#endif


#define NGX_LOG_DEBUG_WASM           NGX_LOG_DEBUG_ALL

#define NGX_WASM_MODULE              0x5741534d   /* "WASM" */
#define NGX_WASM_CONF                0x00300000

#define NGX_WASM_DEFAULT_RUNTIME     "wasmtime"

#define NGX_WASM_OK                  NGX_OK
#define NGX_WASM_ERROR               NGX_ERROR
#define NGX_WASM_BAD_CTX             NGX_DECLINED
#define NGX_WASM_BAD_USAGE           NGX_ABORT
#define NGX_WASM_SENT_LAST           NGX_DONE

#define NGX_WASM_MAX_HOST_TRAP_STR   128


/* wasm vm */


typedef struct ngx_wasm_vm_s  ngx_wasm_vm_t;
typedef struct ngx_wasm_vm_module_s  ngx_wasm_vm_module_t;
typedef struct ngx_wasm_vm_instance_s  ngx_wasm_vm_instance_t;
typedef struct ngx_wasm_vm_func_s  ngx_wasm_vm_func_t;


/* host interface */


#define ngx_wasm_hfunc_null           { ngx_null_string, NULL, NULL, NULL }


typedef struct ngx_wasm_hctx_s  ngx_wasm_hctx_t;
typedef struct ngx_wasm_hfunc_s  ngx_wasm_hfunc_t;

typedef ngx_int_t (*ngx_wasm_hfunc_pt)(ngx_wasm_hctx_t *hctx,
    const wasm_val_t args[], wasm_val_t rets[]);

typedef enum {
    NGX_WASM_HOST_SUBSYS_ANY = 0,
    NGX_WASM_HOST_SUBSYS_HTTP
} ngx_wasm_hsubsys_kind;

struct ngx_wasm_hctx_s {
    ngx_pool_t                    *pool;
    ngx_log_t                     *log;
    ngx_wasm_hsubsys_kind          subsys;
    void                          *data;

    size_t                         trapmsglen;
    u_char                        *trapmsg;
    u_char                        *mem_offset;
};

typedef struct {
    ngx_str_t                      name;
    ngx_wasm_hfunc_pt              ptr;
    const wasm_valkind_t         **args;
    const wasm_valkind_t         **rets;
} ngx_wasm_hdef_func_t;

typedef struct {
    ngx_wasm_hsubsys_kind          subsys;
    ngx_wasm_hdef_func_t          *funcs;
} ngx_wasm_hdefs_t;


extern const wasm_valkind_t * ngx_wasm_arity_i32[];
extern const wasm_valkind_t * ngx_wasm_arity_i32_i32[];
extern const wasm_valkind_t * ngx_wasm_arity_i32_i32_i32[];


/* wasm runtime */


typedef void *ngx_wrt_error_pt;

typedef ngx_int_t (*ngx_wrt_init_pt)(ngx_wasm_vm_t *vm);

typedef void (*ngx_wrt_shutdown_pt)(ngx_wasm_vm_t *vm);

typedef ngx_int_t (*ngx_wrt_wat2wasm_pt)(ngx_wasm_vm_t *vm, u_char *wat,
    size_t len, wasm_byte_vec_t *bytes, ngx_wrt_error_pt *err);

typedef ngx_int_t (*ngx_wrt_module_new_pt)(ngx_wasm_vm_module_t *module,
    wasm_byte_vec_t *bytes, ngx_wrt_error_pt *err);

typedef ngx_int_t (*ngx_wrt_instance_new_pt)(ngx_wasm_vm_instance_t *instance,
    wasm_trap_t **trap, ngx_wrt_error_pt *err);

typedef ngx_int_t (*ngx_wrt_func_call_pt)(wasm_func_t *func, wasm_trap_t **trap,
    ngx_wrt_error_pt *err);

typedef u_char *(*ngx_wrt_error_log_handler_pt)(ngx_wrt_error_pt err,
    u_char *buf, size_t len);

typedef struct {
    ngx_str_t                     name;
    ngx_wrt_init_pt               init;
    ngx_wrt_shutdown_pt           shutdown;
    ngx_wrt_wat2wasm_pt           wat2wasm;
    ngx_wrt_module_new_pt         module_new;
    ngx_wrt_instance_new_pt       instance_new;
    ngx_wrt_func_call_pt          func_call;
    ngx_wrt_error_log_handler_pt  error_log_handler;
} ngx_wrt_t;


/* core wasm module ABI */


typedef struct {
    ngx_wrt_t                     *runtime;
    ngx_wasm_hdefs_t              *hdefs;
    void                          *(*create_conf)(ngx_cycle_t *cycle);
    char                          *(*init_conf)(ngx_cycle_t *cycle, void *conf);
    ngx_int_t                      (*init)(ngx_cycle_t *cycle);
} ngx_wasm_module_t;

ngx_wasm_vm_t *ngx_wasm_core_get_vm(ngx_cycle_t *cycle);

extern ngx_module_t  ngx_wasm_module;


#endif /* _NGX_WASM_H_INCLUDED_ */
