#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm_acl.h>

static const ngx_wasm_acl_type_e  ngx_wasm_acl_allow_tag = NGX_WASM_ACL_ALLOW;
static const ngx_wasm_acl_type_e  ngx_wasm_acl_deny_tag  = NGX_WASM_ACL_DENY;

ngx_wasm_acl_ctx_t *
ngx_wasm_acl_find_ctx(ngx_cycle_t *cycle, ngx_str_t *name)
{
    ngx_uint_t              i;
    ngx_wasm_acl_ctx_t    **acls;
    ngx_wasm_core_conf_t  *wcf;

    wcf = ngx_wasm_core_cycle_get_conf(cycle);
    if (wcf == NULL) {
        return NULL;
    }

    acls = wcf->acls.elts;

    for (i = 0; i < wcf->acls.nelts; i++) {
        if (acls[i]->name.len == name->len
            && ngx_strncmp(acls[i]->name.data, name->data, name->len) == 0)
        {
            return acls[i];
        }
    }

    return NULL;
}


ngx_int_t
ngx_wasm_acl_init(ngx_wasm_acl_t *acl, ngx_wasm_acl_ctx_t *ctx,
    ngx_pool_t *pool)
{
    acl->ctx = ctx;

    if (ngx_array_init(&acl->recorded_addrs, pool, 8, sizeof(ngx_cidr_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static int ngx_libc_cdecl
ngx_wasm_acl_dns_wildcards(const void *one, const void *two)
{
    ngx_hash_key_t  *first, *second;

    first = (ngx_hash_key_t *) one;
    second = (ngx_hash_key_t *) two;

    return ngx_dns_strcmp(first->key.data, second->key.data);
}


ngx_int_t
ngx_wasm_acl_ctx_init(ngx_conf_t *cf, ngx_wasm_acl_ctx_t *acl_ctx)
{
    u_char                     *name;
    ngx_int_t                   rc;
    ngx_uint_t                  i, flag, name_len;
    ngx_hash_init_t             hinit;
    ngx_wasm_acl_host_t        *entries;
    ngx_hash_keys_arrays_t      ha;
    const ngx_wasm_acl_type_e  *type;

    name_len = sizeof("acl_hosts[") - 1 + acl_ctx->name.len + sizeof("]");
    name = ngx_pnalloc(cf->pool, name_len);
    if (name == NULL) {
        return NGX_ERROR;

    }

    ngx_sprintf(name, "acl_hosts[%V]%Z", &acl_ctx->name);

    hinit.name = (char *) name;
    hinit.hash = &acl_ctx->hosts_hash.hash;
    hinit.key = ngx_hash_key_lc;
    hinit.max_size = 512;
    hinit.bucket_size = ngx_align(256, ngx_cacheline_size);
    hinit.pool = cf->pool;
    hinit.temp_pool = cf->temp_pool;

    ngx_memzero(&ha, sizeof(ngx_hash_keys_arrays_t));
    ha.pool = cf->pool;
    ha.temp_pool = cf->temp_pool;

    if (ngx_hash_keys_array_init(&ha, NGX_HASH_SMALL) != NGX_OK) {
        return NGX_ERROR;
    }

    entries = acl_ctx->hosts_arr.elts;

    for (i = 0; i < acl_ctx->hosts_arr.nelts; i++) {
        if (entries[i].wildcard) {
            flag = NGX_HASH_WILDCARD_KEY;
        } else {
            flag = 0;
        }

        switch (entries[i].type) {
        case NGX_WASM_ACL_ALLOW:
            type = &ngx_wasm_acl_allow_tag;
            break;

        case NGX_WASM_ACL_DENY:
            type = &ngx_wasm_acl_deny_tag;
            break;
        }

        rc = ngx_hash_add_key(&ha, &entries[i].host,
                              (void *) type, flag);
        if (rc == NGX_BUSY) {
              ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                 "duplicate host \"%V\" in acl",
                                 &entries[i].host);
              return NGX_ERROR;
          }
        if (rc == NGX_DECLINED) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                             "invalid wildcard host \"%V\" in acl",
                               &entries[i].host);
            return NGX_ERROR;
        }
        if (rc != NGX_OK) {
            return NGX_ERROR;
        }
    }

    if (ngx_hash_init(&hinit, ha.keys.elts, ha.keys.nelts) != NGX_OK) {
        return NGX_ERROR;
    }

    if (ha.dns_wc_tail.nelts) {
        ngx_qsort(ha.dns_wc_tail.elts,
                  (size_t) ha.dns_wc_tail.nelts,
                  sizeof(ngx_hash_key_t),
                  ngx_wasm_acl_dns_wildcards);

        hinit.hash = NULL;
        hinit.temp_pool = ha.temp_pool;

        if (ngx_hash_wildcard_init(&hinit, ha.dns_wc_tail.elts,
                                   ha.dns_wc_tail.nelts)
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        acl_ctx->hosts_hash.wc_tail = (ngx_hash_wildcard_t *) hinit.hash;
    }

    if (ha.dns_wc_head.nelts) {
        ngx_qsort(ha.dns_wc_head.elts,
                  (size_t) ha.dns_wc_head.nelts,
                  sizeof(ngx_hash_key_t),
                  ngx_wasm_acl_dns_wildcards);

        hinit.hash = NULL;
        hinit.temp_pool = ha.temp_pool;

        if (ngx_hash_wildcard_init(&hinit, ha.dns_wc_head.elts,
                                   ha.dns_wc_head.nelts)
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        acl_ctx->hosts_hash.wc_head = (ngx_hash_wildcard_t *) hinit.hash;
    }

    return NGX_OK;
}


ngx_int_t
ngx_wasm_acl_ctx_insert_cidr(ngx_wasm_acl_ctx_t *acl_ctx, ngx_cidr_t *cidr, ngx_wasm_acl_type_e type)
{
    uintptr_t  v;

    switch (type) {
    case NGX_WASM_ACL_ALLOW:
        v = (uintptr_t) &ngx_wasm_acl_allow_tag;
        break;

    case NGX_WASM_ACL_DENY:
        v = (uintptr_t) &ngx_wasm_acl_deny_tag;
        break;

    default:
        return NGX_ERROR;
    }

    switch (cidr->family) {
#if (NGX_HAVE_INET6)
    case AF_INET6:
        return ngx_radix128tree_insert(acl_ctx->addrs_v6, cidr->u.in6.addr.s6_addr,
                                     cidr->u.in6.mask.s6_addr, v);

#endif
    default:
        cidr->u.in.addr = ntohl(cidr->u.in.addr);
        cidr->u.in.mask = ntohl(cidr->u.in.mask);

        return ngx_radix32tree_insert(acl_ctx->addrs_v4, cidr->u.in.addr,
                                      cidr->u.in.mask, v);
    }
}


static uintptr_t
ngx_wasm_acl_tree_lookup(ngx_wasm_acl_ctx_t *ctx, struct sockaddr *addr)
{
    u_char   *p6;
    uint32_t  v4;

    switch (addr->sa_family) {

#if (NGX_HAVE_INET6)
    case AF_INET6:
        p6 = ((struct sockaddr_in6 *) addr)->sin6_addr.s6_addr;
        return ngx_radix128tree_find(ctx->addrs_v6, p6);
#endif

    case AF_INET:
        v4 = ntohl(((struct sockaddr_in *) addr)->sin_addr.s_addr);
        return ngx_radix32tree_find(ctx->addrs_v4, v4);

    default:
        return NGX_RADIX_NO_VALUE;
    }
}


ngx_int_t
ngx_wasm_acl_check_addr(ngx_wasm_acl_t *acl, struct sockaddr *addr)
{
    uintptr_t                   v;
    const ngx_wasm_acl_type_e  *type;

    v = ngx_wasm_acl_tree_lookup(acl->ctx, addr);

    if (v != NGX_RADIX_NO_VALUE) {
        type = (const ngx_wasm_acl_type_e *) v;
        switch (*type) {
        case NGX_WASM_ACL_ALLOW:
            return NGX_OK;
        case NGX_WASM_ACL_DENY:
            return NGX_DECLINED;
        }
    }

    if (ngx_cidr_match(addr, &acl->recorded_addrs) == NGX_OK) {
        return NGX_OK;
    }

    return NGX_DECLINED;
}


ngx_int_t
ngx_wasm_acl_check_host(ngx_wasm_acl_t *acl, ngx_str_t *host)
{
    u_char               *p;
    in_addr_t             v4;
    ngx_str_t             name;
    ngx_uint_t            key;
    ngx_sockaddr_t        sa;
    ngx_wasm_acl_type_e  *type;
    u_char                buf[NGX_MAXHOSTNAMELEN];

    if (host->len == 0) {
        return NGX_ERROR;
    }

    if (host->data[0] == '[') {
        p = ngx_strlchr(host->data, host->data + host->len, ']');
        if (p == NULL) {
            return NGX_DECLINED;
        }

        name.data = host->data + 1;
        name.len  = p - host->data - 1;

    } else {
        p = ngx_strlchr(host->data, host->data + host->len, ':');
        name.data = host->data;
        name.len  = p ? (size_t) (p - host->data) : host->len;
    }

    ngx_memzero(&sa, sizeof(ngx_sockaddr_t));

    v4 = ngx_inet_addr(name.data, name.len);
    if (v4 != INADDR_NONE) {
        sa.sockaddr_in.sin_family = AF_INET;
        sa.sockaddr_in.sin_addr.s_addr = v4;
        return ngx_wasm_acl_check_addr(acl, &sa.sockaddr);
    }

#if (NGX_HAVE_INET6)
    if (ngx_inet6_addr(name.data, name.len,
                       sa.sockaddr_in6.sin6_addr.s6_addr) == NGX_OK)
    {
        sa.sockaddr_in6.sin6_family = AF_INET6;
        return ngx_wasm_acl_check_addr(acl, &sa.sockaddr);
    }
#endif

    if (name.len == 0 || name.len > sizeof(buf)) {
        return NGX_DECLINED;
    }

    ngx_strlow(buf, name.data, name.len);
    key = ngx_hash_key_lc(buf, name.len);

    type = ngx_hash_find_combined(&acl->ctx->hosts_hash, key, buf, name.len);
    if (type == NULL) {
        return NGX_DECLINED;
    }

    switch (*type) {
    case NGX_WASM_ACL_ALLOW: return NGX_OK;
    case NGX_WASM_ACL_DENY: return NGX_DECLINED;
    }

    return NGX_DECLINED;
}


static ngx_int_t
ngx_wasm_sockaddr_to_cidr(struct sockaddr *sa, ngx_cidr_t *cidr)
{
    switch (sa->sa_family) {

    case AF_INET:
        cidr->family = AF_INET;
        cidr->u.in.addr = ((struct sockaddr_in *) sa)->sin_addr.s_addr;
        cidr->u.in.mask = 0xffffffff;
        return NGX_OK;

#if (NGX_HAVE_INET6)
    case AF_INET6:
        cidr->family = AF_INET6;
        ngx_memcpy(cidr->u.in6.addr.s6_addr,
                   ((struct sockaddr_in6 *) sa)->sin6_addr.s6_addr, 16);
        ngx_memset(cidr->u.in6.mask.s6_addr, 0xff, 16);
        return NGX_OK;
#endif
    }

    return NGX_DECLINED;
}


ngx_int_t
ngx_wasm_acl_add_addr(ngx_wasm_acl_t *acl, struct sockaddr *addr)
{
    uintptr_t                   v;
    ngx_cidr_t                  cidr, *slot;
    const ngx_wasm_acl_type_e  *type;

    v = ngx_wasm_acl_tree_lookup(acl->ctx, addr);

    if (v != NGX_RADIX_NO_VALUE) {
        type = (const ngx_wasm_acl_type_e  *) v;
        if (*type == NGX_WASM_ACL_DENY) {
            return NGX_DECLINED;
        }
    }

    if (ngx_wasm_sockaddr_to_cidr(addr, &cidr) != NGX_OK) {
        return NGX_ERROR;
    }

    slot = ngx_array_push(&acl->recorded_addrs);
    if (slot == NULL) {
        return NGX_ERROR;
    }

    *slot = cidr;

    return NGX_OK;
}
