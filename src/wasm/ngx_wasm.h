#ifndef _NGX_WASM_H_INCLUDED_
#define _NGX_WASM_H_INCLUDED_


#include <ngx_core.h>

#include <ngx_wrt.h>

#if (NGX_DEBUG)
#include <assert.h>
#   define ngx_wasm_assert(a)        assert(a)
#else
#   define ngx_wasm_assert(a)
#endif


#define NGX_LOG_DEBUG_WASM           NGX_LOG_DEBUG_ALL

#define NGX_WASM_MODULE              0x5741534d   /* "WASM" */
#define NGX_WASM_CONF                0x00300000

#define NGX_WASM_CONF_ERR_NO_WASM    "is specified but config has no \"wasm\" section"


/* wasm vm */


#define NGX_WAVM_OK                  NGX_OK
#define NGX_WAVM_ERROR               NGX_ERROR
#define NGX_WAVM_BAD_CTX             NGX_DECLINED
#define NGX_WAVM_BAD_USAGE           NGX_ABORT
#define NGX_WAVM_SENT_LAST           NGX_DONE


typedef struct ngx_wavm_s  ngx_wavm_t;
typedef struct ngx_wavm_module_s  ngx_wavm_module_t;
typedef struct ngx_wavm_ctx_s  ngx_wavm_ctx_t;
typedef struct ngx_wavm_instance_s  ngx_wavm_instance_t;
typedef struct ngx_wavm_func_s  ngx_wavm_func_t;
typedef struct ngx_wavm_hfunc_def_s  ngx_wavm_hfunc_def_t;


/* subsystems */


typedef struct {
    ngx_str_t                    name;
    ngx_uint_t                   index;
    ngx_uint_t                   on;
} ngx_wasm_phase_t;


typedef struct {
    ngx_uint_t                   nphases;
    ngx_wasm_phase_t            *phases;
} ngx_wasm_subsystem_t;


/* core wasm module ABI */


#define ngx_wasm_core_cycle_get_conf(cycle)                                  \
    (ngx_get_conf(cycle->conf_ctx, ngx_wasm_module))                         \
    ? (*(ngx_get_conf(cycle->conf_ctx, ngx_wasm_module)))                    \
      [ngx_wasm_core_module.ctx_index]                                       \
    : NULL


typedef struct {
    ngx_wavm_hfunc_def_t        *hfuncs;
    void                        *(*create_conf)(ngx_cycle_t *cycle);
    char                        *(*init_conf)(ngx_cycle_t *cycle, void *conf);
    ngx_int_t                    (*init)(ngx_cycle_t *cycle);
} ngx_wasm_module_t;


ngx_wavm_t *ngx_wasm_core_get_vm(ngx_cycle_t *cycle);

ngx_int_t ngx_wasm_bytes_from_path(wasm_byte_vec_t *out, u_char *path,
    ngx_log_t *log);

void ngx_wasm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
#if (NGX_HAVE_VARIADIC_MACROS)
    const char *fmt, ...);

#else
    const char *fmt, va_list args);
#endif

extern ngx_uint_t  ngx_wasm_max_module;
extern ngx_module_t  ngx_wasm_module;


/* ngx_str_node_t extensions */


#define ngx_wasm_sn_rbtree_insert(rbtree, sn)                                \
    ngx_rbtree_insert((rbtree), &(sn)->node)

#define ngx_wasm_sn_init(sn, s)                                              \
    (sn)->node.key = ngx_crc32_long((s)->data, (s)->len);                    \
    (sn)->str.len = (s)->len;                                                \
    (sn)->str.data = (s)->data

#define ngx_wasm_sn_n2sn(n)                                                  \
    (ngx_str_node_t *) ((u_char *) (n) - offsetof(ngx_str_node_t, node))

#define ngx_wasm_sn_sn2data(sn, type, member)                                \
    (type *) ((u_char *) &(sn)->node - offsetof(type, member))


static ngx_inline ngx_str_node_t *
ngx_wasm_sn_rbtree_lookup(ngx_rbtree_t *rbtree, ngx_str_t *str)
{
    uint32_t   hash;

    hash = ngx_crc32_long(str->data, str->len);

    return ngx_str_rbtree_lookup(rbtree, str, hash);
}


#endif /* _NGX_WASM_H_INCLUDED_ */
