#ifndef _NGX_WA_H_INCLUDED_
#define _NGX_WA_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_wa_shm.h>
#include <ngx_wa_metrics.h>


#if (NGX_DEBUG)
#include <assert.h>
#   define ngx_wa_assert(a)         assert(a)
#else
#   define ngx_wa_assert(a)
#endif

#define NGX_WA_BAD_FD               (ngx_socket_t) -1

#define NGX_WA_WASM_CONF_OFFSET     offsetof(ngx_wa_conf_t, wasm_confs)
#define NGX_WA_IPC_CONF_OFFSET      offsetof(ngx_wa_conf_t, ipc_confs)

#define NGX_WA_CONF_ERR_DUPLICATE   "is duplicate"

#define NGX_WA_METRICS_DEFAULT_MAX_NAME_LEN  256
#define NGX_WA_METRICS_DEFAULT_SLAB_SIZE     1024 * 1024 * 5   /* 5 MiB */


/* ngx_wasmx_module */


#define ngx_wa_cycle_get_conf(cycle)                                         \
    (ngx_wa_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_wasmx_module)


typedef void *(*ngx_wa_create_conf_pt)(ngx_conf_t *cf);
typedef char *(*ngx_wa_init_conf_pt)(ngx_conf_t *cf, void *conf);
typedef ngx_int_t (*ngx_wa_init_pt)(ngx_cycle_t *cycle);


typedef struct {
    ngx_uint_t               initialized_types;
    void                   **wasm_confs;
#ifdef NGX_WA_IPC
    void                   **ipc_confs;
#endif
    ngx_array_t              shms;     /* ngx_wa_shm_mapping_t */
    ngx_wa_metrics_t        *metrics;
} ngx_wa_conf_t;


ngx_array_t *ngx_wasmx_shms(ngx_cycle_t *cycle);
ngx_wa_metrics_t *ngx_wasmx_metrics(ngx_cycle_t *cycle);


extern ngx_module_t  ngx_wasmx_module;

extern ngx_uint_t    ngx_wasm_max_module;
extern ngx_uint_t    ngx_ipc_max_module;


/* ngx_str_node_t extensions */


#define ngx_wa_sn_insert(rbtree, sn)                                         \
    ngx_rbtree_insert((rbtree), &(sn)->node)

#define ngx_wa_sn_init(sn, s)                                                \
    (sn)->node.key = ngx_crc32_long((s)->data, (s)->len);                    \
    (sn)->str.len = (s)->len;                                                \
    (sn)->str.data = (s)->data

#define ngx_wa_sn_n2sn(n)                                                    \
    (ngx_str_node_t *) ((u_char *) (n) - offsetof(ngx_str_node_t, node))


static ngx_inline ngx_str_node_t *
ngx_wa_sn_lookup(ngx_rbtree_t *rbtree, ngx_str_t *str)
{
    uint32_t   hash;

    hash = ngx_crc32_long(str->data, str->len);

    return ngx_str_rbtree_lookup(rbtree, str, hash);
}


/* ngx_str_t extensions */


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


#endif /* _NGX_WA_H_INCLUDED_ */
