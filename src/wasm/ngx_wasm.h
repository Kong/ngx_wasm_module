#ifndef _NGX_WASM_H_INCLUDED_
#define _NGX_WASM_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wrt.h>
#if (NGX_SSL)
#include <ngx_wasm_ssl.h>
#endif

#if (NGX_DEBUG)
#include <assert.h>
#   define ngx_wasm_assert(a)        assert(a)
#else
#   define ngx_wasm_assert(a)
#endif

#define NGX_LOG_DEBUG_WASM           NGX_LOG_DEBUG_ALL
#define NGX_LOG_WASM_NYI             NGX_LOG_ALERT

#define NGX_WASM_MODULE              0x5741534d   /* "WASM" */
#define NGX_WASM_CONF                0x00300000

#define NGX_WASM_DONE_PHASE          15

#define NGX_WASM_BAD_FD              (ngx_socket_t) -1

#define NGX_WASM_CONF_ERR_DUPLICATE  "is duplicate"
#define NGX_WASM_CONF_ERR_NO_WASM                                            \
    "is specified but config has no \"wasm\" section"

#define ngx_wasm_core_cycle_get_conf(cycle)                                  \
    (ngx_get_conf(cycle->conf_ctx, ngx_wasm_module))                         \
    ? (*(ngx_get_conf(cycle->conf_ctx, ngx_wasm_module)))                    \
      [ngx_wasm_core_module.ctx_index]                                       \
    : NULL

#define ngx_wasm_vec_set_i32(vec, i, v)                                      \
    (((wasm_val_vec_t *) (vec))->data[i].kind = WASM_I32);                   \
     ((wasm_val_vec_t *) (vec))->data[i].of.i32 = v
#define ngx_wasm_vec_set_i64(vec, i, v)                                      \
    (((wasm_val_vec_t *) (vec))->data[i].kind = WASM_I64);                   \
     ((wasm_val_vec_t *) (vec))->data[i].of.i64 = v
#define ngx_wasm_vec_set_f32(vec, i, v)                                      \
    (((wasm_val_vec_t *) (vec))->data[i].kind = WASM_F32);                   \
     ((wasm_val_vec_t *) (vec))->data[i].of.f32 = v
#define ngx_wasm_vec_set_f64(vec, i, v)                                      \
    (((wasm_val_vec_t *) (vec))->data[i].kind = WASM_F64);                   \
     ((wasm_val_vec_t *) (vec))->data[i].of.f64 = v


typedef struct ngx_wavm_s  ngx_wavm_t;
typedef struct ngx_wavm_store_s  ngx_wavm_store_t;
typedef struct ngx_wavm_module_s  ngx_wavm_module_t;
typedef struct ngx_wavm_funcref_s  ngx_wavm_funcref_t;
typedef struct ngx_wavm_func_s  ngx_wavm_func_t;
typedef uint32_t  ngx_wavm_ptr_t;


typedef struct {
    void                        *(*create_conf)(ngx_cycle_t *cycle);
    char                        *(*init_conf)(ngx_cycle_t *cycle, void *conf);
    ngx_int_t                    (*init)(ngx_cycle_t *cycle);
} ngx_wasm_module_t;


ngx_wavm_t *ngx_wasm_main_vm(ngx_cycle_t *cycle);
size_t ngx_wasm_chain_len(ngx_chain_t *in, unsigned *eof);
ngx_uint_t ngx_wasm_chain_clear(ngx_chain_t *in, size_t offset, unsigned *eof,
    unsigned *flush);
ngx_chain_t *ngx_wasm_chain_get_free_buf(ngx_pool_t *p,
    ngx_chain_t **free, size_t len, ngx_buf_tag_t tag, unsigned reuse);
ngx_int_t ngx_wasm_chain_prepend(ngx_pool_t *pool, ngx_chain_t **in,
    ngx_str_t *str, ngx_chain_t **free, ngx_buf_tag_t tag);
ngx_int_t ngx_wasm_chain_append(ngx_pool_t *pool, ngx_chain_t **in, size_t at,
    ngx_str_t *str, ngx_chain_t **free, ngx_buf_tag_t tag);
ngx_uint_t ngx_wasm_list_nelts(ngx_list_t *list);
ngx_int_t ngx_wasm_bytes_from_path(wasm_byte_vec_t *out, u_char *path,
    ngx_log_t *log);
ngx_str_t *ngx_wasm_get_list_elem(ngx_list_t *map, u_char *key, size_t key_len);
#if 0
ngx_int_t ngx_wasm_add_list_elem(ngx_pool_t *pool, ngx_list_t *map,
    u_char *key, size_t key_len, u_char *value, size_t val_len);
ngx_connection_t *ngx_wasm_connection_create(ngx_pool_t *pool);
void ngx_wasm_connection_destroy(ngx_connection_t *c);
#endif

void ngx_wasm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...);
ngx_msec_t ngx_wasm_monotonic_time();

extern ngx_module_t  ngx_wasm_module;
extern ngx_uint_t  ngx_wasm_max_module;


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


static ngx_inline unsigned
ngx_str_eq(const void *s1, ngx_int_t s1_len, const void *s2, ngx_int_t s2_len)
{
    if (s1_len < 0) {
        s1_len = ngx_strlen((const char *) s1);
    }

    if (s2_len < 0) {
        s2_len = ngx_strlen((const char *) s2);
    }

    if (s1_len != s2_len) {
        return 0;
    }

    return ngx_memcmp(s1, s2, s2_len) == 0;
}


/* subsystems & phases */


typedef struct {
    ngx_str_t                                name;
    ngx_uint_t                               index;
    ngx_uint_t                               on;
} ngx_wasm_phase_t;


typedef struct {
    ngx_uint_t                               nphases;
    ngx_wasm_phase_t                        *phases;
} ngx_wasm_subsystem_t;


#endif /* _NGX_WASM_H_INCLUDED_ */
