#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_proxy_wasm.h>
#include <ngx_proxy_wasm_properties.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_proxy_wasm.h>
#endif


#define ngx_proxy_wasm_store_init(s, p)                                      \
    (s)->pool = (p);                                                         \
    ngx_queue_init(&(s)->sweep);                                             \
    ngx_queue_init(&(s)->free);                                              \
    ngx_queue_init(&(s)->busy)


static ngx_proxy_wasm_err_e ngx_proxy_wasm_create_context(
    ngx_proxy_wasm_filter_t *filter, ngx_proxy_wasm_ctx_t *pwctx,
    ngx_uint_t id, ngx_proxy_wasm_exec_t *in, ngx_proxy_wasm_exec_t **out);
static void ngx_proxy_wasm_on_log(ngx_proxy_wasm_exec_t *pwexec);
static void ngx_proxy_wasm_on_done(ngx_proxy_wasm_exec_t *pwexec);
static ngx_int_t ngx_proxy_wasm_on_tick(ngx_proxy_wasm_exec_t *pwexec);
static ngx_proxy_wasm_filter_t *ngx_proxy_wasm_lookup_filter(
    ngx_proxy_wasm_filters_root_t *pwroot, ngx_uint_t id);
static ngx_proxy_wasm_exec_t *ngx_proxy_wasm_lookup_root_ctx(
    ngx_proxy_wasm_instance_t *ictx, ngx_uint_t id);
static ngx_proxy_wasm_exec_t *ngx_proxy_wasm_lookup_ctx(
    ngx_proxy_wasm_instance_t *ictx, ngx_uint_t id);
static ngx_int_t ngx_proxy_wasm_filter_init_abi(
    ngx_proxy_wasm_filter_t *filter);
static ngx_int_t ngx_proxy_wasm_filter_start(ngx_proxy_wasm_filter_t *filter);
static void ngx_proxy_wasm_instance_update(
    ngx_proxy_wasm_instance_t *ictx, ngx_proxy_wasm_exec_t *pwexec);
static void ngx_proxy_wasm_instance_invalidate(ngx_proxy_wasm_instance_t *ictx);
static void ngx_proxy_wasm_instance_destroy(ngx_proxy_wasm_instance_t *ictx);
static void ngx_proxy_wasm_store_destroy(ngx_proxy_wasm_store_t *store);
static void ngx_proxy_wasm_store_sweep(ngx_proxy_wasm_store_t *store);
#if 0
static void ngx_proxy_wasm_store_schedule_sweep_handler(ngx_event_t *ev);
static void ngx_proxy_wasm_store_schedule_sweep(ngx_proxy_wasm_store_t *store);
#endif


static ngx_uint_t  next_id = 0;


/* context - root */


ngx_proxy_wasm_filters_root_t *
ngx_proxy_wasm_root_alloc(ngx_pool_t *pool)
{
    ngx_proxy_wasm_filters_root_t *pwroot;

    pwroot = ngx_pcalloc(pool, sizeof(ngx_proxy_wasm_filters_root_t));
    if (pwroot == NULL) {
        return NULL;
    }

    /**
     * Note: potential for creating a new pwroot->pool in similar fashion
     * to ngx_proxy_wasm_ctx_t.
     */

    return pwroot;
}


void
ngx_proxy_wasm_root_init(ngx_proxy_wasm_filters_root_t *pwroot,
    ngx_pool_t *pool)
{
    if (pwroot->init) {
        return;
    }

    pwroot->init = 1;

    ngx_array_init(&pwroot->filter_ids, pool, 0, sizeof(ngx_uint_t));

    ngx_rbtree_init(&pwroot->tree, &pwroot->sentinel,
                    ngx_rbtree_insert_value);

    ngx_proxy_wasm_store_init(&pwroot->store, pool);
}


void
ngx_proxy_wasm_root_destroy(ngx_proxy_wasm_filters_root_t *pwroot)
{
    ngx_rbtree_node_t        **root, **sentinel, *node;
    ngx_proxy_wasm_filter_t   *filter;

    root = &pwroot->tree.root;
    sentinel = &pwroot->tree.sentinel;

    while (*root != *sentinel) {
        node = ngx_rbtree_min(*root, *sentinel);
        filter = ngx_rbtree_data(node, ngx_proxy_wasm_filter_t, node);

        ngx_rbtree_delete(&pwroot->tree, node);

        ngx_proxy_wasm_store_destroy(filter->store);
    }

    ngx_proxy_wasm_store_destroy(&pwroot->store);

    ngx_array_destroy(&pwroot->filter_ids);
}


static ngx_uint_t
get_filter_id(ngx_str_t *name, ngx_str_t *config, uintptr_t data)
{
    u_char      buf[NGX_INT64_LEN];
    uint32_t    hash;
    ngx_str_t   str;

    str.data = buf;
    str.len = ngx_sprintf(str.data, "%ld", data) - str.data;

    ngx_crc32_init(hash);
    ngx_crc32_update(&hash, name->data, name->len);
    ngx_crc32_update(&hash, config->data, config->len);
    ngx_crc32_update(&hash, str.data, str.len);
    ngx_crc32_final(hash);

    return hash;
}


ngx_int_t
ngx_proxy_wasm_load(ngx_proxy_wasm_filters_root_t *pwroot,
    ngx_proxy_wasm_filter_t *filter, ngx_log_t *log)
{
    dd("enter");

    if (filter->loaded) {
        dd("loaded");
        return NGX_OK;
    }

    if (filter->module == NULL) {
        filter->ecode = NGX_PROXY_WASM_ERR_BAD_HOST_INTERFACE;
        return NGX_ERROR;
    }

    filter->log = log;
    filter->name = &filter->module->name;
    filter->id = get_filter_id(filter->name, &filter->config, filter->data);

    dd("insert \"%.*s\" filter in pwroot: %p (config: \"%.*s\", id: %ld)",
       (int) filter->name->len, filter->name->data, pwroot,
       (int) filter->config.len, filter->config.data, filter->id);

    filter->node.key = filter->id;
    ngx_rbtree_insert(&pwroot->tree, &filter->node);

    if (ngx_proxy_wasm_filter_init_abi(filter) != NGX_OK) {
        return NGX_ERROR;
    }

    filter->loaded = 1;

    dd("exit");

    return NGX_OK;
}


ngx_int_t
ngx_proxy_wasm_start(ngx_proxy_wasm_filters_root_t *pwroot)
{
    ngx_int_t                 rc;
    ngx_rbtree_node_t        *root, *sentinel, *node;
    ngx_proxy_wasm_filter_t  *filter;

    dd("enter (pwroot: %p)", pwroot);

    root = pwroot->tree.root;
    sentinel = pwroot->tree.sentinel;

    if (root == sentinel) {
        dd("no filters");
        goto done;
    }

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&pwroot->tree, node))
    {
        filter = ngx_rbtree_data(node, ngx_proxy_wasm_filter_t, node);

        rc = ngx_proxy_wasm_filter_start(filter);
        if (rc != NGX_OK) {
            ngx_proxy_wasm_log_error(NGX_LOG_EMERG, filter->log, filter->ecode,
                                     "failed initializing \"%V\" filter",
                                     filter->name);
            return NGX_ERROR;
        }
    }

done:

    return NGX_OK;
}


/* context - stream */


ngx_proxy_wasm_ctx_t *
ngx_proxy_wasm_ctx_alloc(ngx_pool_t *pool)
{
    ngx_proxy_wasm_ctx_t  *pwctx;

    pwctx = ngx_pcalloc(pool, sizeof(ngx_proxy_wasm_ctx_t));
    if (pwctx == NULL) {
        return NULL;
    }

    pwctx->pool = ngx_create_pool(512, pool->log);
    if (pwctx->pool == NULL) {
        return NULL;
    }

    pwctx->parent_pool = pool;

    ngx_rbtree_init(&pwctx->host_props_tree, &pwctx->host_props_sentinel,
                    ngx_str_rbtree_insert_value);

    return pwctx;
}


ngx_proxy_wasm_ctx_t *
ngx_proxy_wasm_ctx(ngx_proxy_wasm_filters_root_t *pwroot,
    ngx_array_t *filter_ids, ngx_uint_t isolation,
    ngx_proxy_wasm_subsystem_t *subsys, void *data)
{
    size_t                    i;
    ngx_uint_t                id;
    ngx_proxy_wasm_ctx_t     *pwctx;
    ngx_proxy_wasm_filter_t  *filter;
    ngx_proxy_wasm_exec_t    *pwexec = NULL;

    pwctx = subsys->get_context(data);
    if (pwctx == NULL) {
        return NULL;
    }

    if (!pwctx->init) {
        if (isolation == NGX_PROXY_WASM_ISOLATION_STREAM) {
            ngx_proxy_wasm_store_init(&pwctx->store, pwctx->pool);
        }

        pwctx->init = 1;
    }

    if (!pwctx->ready && filter_ids) {
        ngx_wa_assert(pwroot);

        pwctx->isolation = isolation;
        pwctx->nfilters = filter_ids->nelts;

        ngx_log_debug2(NGX_LOG_DEBUG_WASM, pwctx->log, 0,
                       "proxy_wasm initializing filter chain "
                       "(nfilters: %l, isolation: %ui)",
                       pwctx->nfilters, pwctx->isolation);

        ngx_array_init(&pwctx->pwexecs, pwctx->pool, pwctx->nfilters,
                       sizeof(ngx_proxy_wasm_exec_t));

        for (i = 0; i < filter_ids->nelts; i++) {
            id = ((ngx_uint_t *) filter_ids->elts)[i];

            filter = ngx_proxy_wasm_lookup_filter(pwroot, id);

            if (filter == NULL) {
                /* TODO: log error */
                ngx_wa_assert(0);
                return NULL;
            }

            (void) ngx_proxy_wasm_create_context(filter, pwctx, ++next_id,
                                                 NULL, &pwexec);
            if (pwexec == NULL) {
                return NULL;
            }

        }  /* for () */

        pwctx->ready = 1;

    }  /* !ready */

    return pwctx;
}


static void
destroy_pwexec(ngx_proxy_wasm_exec_t *pwexec)
{

#if 1
    /* never called on root rexec for now */
    ngx_wa_assert(pwexec->root_id != NGX_PROXY_WASM_ROOT_CTX_ID);

#else
    if (pwexec->ev) {
        ngx_del_timer(pwexec->ev);
        ngx_free(pwexec->ev);
        pwexec->ev = NULL;
    }
#endif

    pwexec->ictx = NULL;
    pwexec->started = 0;

    if (pwexec->log) {
        if (pwexec->log_ctx.log_prefix.data) {
            ngx_pfree(pwexec->pool, pwexec->log_ctx.log_prefix.data);
        }

        ngx_pfree(pwexec->pool, pwexec->log);
        pwexec->log = NULL;
    }

    if (pwexec->parent) {
        if (pwexec->log_ctx.log_prefix.data) {
            ngx_pfree(pwexec->pool, pwexec->log_ctx.log_prefix.data);
            pwexec->log_ctx.log_prefix.data = NULL;
        }

#if 0
        /* never called on root rexec for now */
        if (pwexec->root_id == NGX_PROXY_WASM_ROOT_CTX_ID) {
            ngx_pfree(pwexec->pool, pwexec->parent);
            pwexec->parent = NULL;

        } else
#endif
        if (pwexec->parent->isolation == NGX_PROXY_WASM_ISOLATION_FILTER
            && pwexec->store)
        {
            /* store specifically allocated for the pwexec */
            ngx_pfree(pwexec->parent->pool, pwexec->store);
            pwexec->store = NULL;
        }
    }
}


void
ngx_proxy_wasm_ctx_destroy(ngx_proxy_wasm_ctx_t *pwctx)
{
    size_t                  i;
    ngx_proxy_wasm_exec_t  *pwexec, *pwexecs;

    dd("enter (pwctx: %p)", pwctx);

    if (pwctx->ready
        && pwctx->isolation == NGX_PROXY_WASM_ISOLATION_STREAM)
    {
        ngx_proxy_wasm_store_destroy(&pwctx->store);
    }

    pwexecs = (ngx_proxy_wasm_exec_t *) pwctx->pwexecs.elts;

    for (i = 0; i < pwctx->pwexecs.nelts; i++) {
        pwexec = &pwexecs[i];

        ngx_wa_assert(pwexec->root_id != NGX_PROXY_WASM_ROOT_CTX_ID);

        ngx_proxy_wasm_log_error(NGX_LOG_DEBUG, pwctx->log, 0,
                                 "\"%V\" filter freeing context #%d "
                                 "(%l/%l)",
                                 pwexec->filter->name, pwexec->id,
                                 pwexec->index + 1, pwctx->nfilters);

        if (pwexec->ictx) {
            if (pwexec->node.key) {
                ngx_rbtree_delete(&pwexec->ictx->tree_ctxs, &pwexec->node);
            }

            if (pwctx->isolation == NGX_PROXY_WASM_ISOLATION_NONE) {
                /* sweep if an instance has trapped */
                ngx_proxy_wasm_store_sweep(pwexec->ictx->store);

            } else if (pwctx->isolation == NGX_PROXY_WASM_ISOLATION_FILTER) {
                /* destroy filter context store */
                ngx_proxy_wasm_store_destroy(pwexec->store);
            }
        }

        destroy_pwexec(pwexec);
    }

    if (pwctx->ready) {
        ngx_array_destroy(&pwctx->pwexecs);
    }

#if 0
    if (pwctx->authority.data) {
        ngx_pfree(pwctx->pool, pwctx->authority.data);
    }

    if (pwctx->scheme.data) {
        ngx_pfree(pwctx->pool, pwctx->scheme.data);
    }

    if (pwctx->path.data) {
        ngx_pfree(pwctx->pool, pwctx->path.data);
    }

    if (pwctx->start_time.data) {
        ngx_pfree(pwctx->pool, pwctx->start_time.data);
    }

    if (pwctx->upstream_address.data) {
        ngx_pfree(pwctx->pool, pwctx->upstream_address.data);
    }

    if (pwctx->upstream_port.data) {
        ngx_pfree(pwctx->pool, pwctx->upstream_port.data);
    }

    if (pwctx->connection_id.data) {
        ngx_pfree(pwctx->pool, pwctx->connection_id.data);
    }

    if (pwctx->mtls.data) {
        ngx_pfree(pwctx->pool, pwctx->mtls.data);
    }

    if (pwctx->root_id.data) {
        ngx_pfree(pwctx->pool, pwctx->root_id.data);
    }

    if (pwctx->call_status.data) {
        ngx_pfree(pwctx->pool, pwctx->call_status.data);
    }

    if (pwctx->response_status.data) {
        ngx_pfree(pwctx->pool, pwctx->response_status.data);
    }
#endif

    ngx_destroy_pool(pwctx->pool);

    ngx_pfree(pwctx->parent_pool, pwctx);
}


static ngx_int_t
action2rc(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_proxy_wasm_exec_t *pwexec)
{
    ngx_int_t                 rc = NGX_ERROR;
    ngx_proxy_wasm_exec_t    *pwexecs;
    ngx_proxy_wasm_filter_t  *filter;
    ngx_proxy_wasm_action_e   action;
#ifdef NGX_WASM_HTTP
    ngx_wavm_instance_t      *instance;
    ngx_http_wasm_req_ctx_t  *rctx;
#endif

    filter = pwexec->filter;
    action = pwctx->action;

    dd("enter (pwexec: %p, action: %d)", pwexec, action);

    ngx_wa_assert(pwexec->root_id != NGX_PROXY_WASM_ROOT_CTX_ID);

    if (pwexec->ecode) {
        if (!pwexec->ecode_logged
            && pwctx->step != NGX_PROXY_WASM_STEP_DONE)
        {
            ngx_proxy_wasm_log_error(NGX_LOG_INFO, pwctx->log, pwexec->ecode,
                                     "filter chain failed resuming: "
                                     "previous error");

            pwexec->ecode_logged = 1;
        }
#if (NGX_DEBUG)
        else {
            ngx_proxy_wasm_log_error(NGX_LOG_DEBUG, pwctx->log,
                                     pwexec->ecode,
                                     "filter %l/%l skipping \"%V\" "
                                     "step in \"%V\" phase: previous error",
                                     pwexec->index + 1, pwctx->nfilters,
                                     ngx_proxy_wasm_step_name(pwctx->step),
                                     &pwctx->phase->name);
        }
#endif

        rc = filter->subsystem->ecode(pwexec->ecode);
        goto error;
    }

    /* exceptioned steps */

    switch (pwctx->step) {
    case NGX_PROXY_WASM_STEP_DONE:
        /* force-resume the done step */
        goto cont;
    default:
        break;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_WASM, pwctx->log, 0, "proxy_wasm return "
                   "action: \"%V\"", ngx_proxy_wasm_action_name(action));

    /* determine current action rc */

    switch (action) {

    case NGX_PROXY_WASM_ACTION_DONE:
        ngx_log_debug0(NGX_LOG_DEBUG_WASM, pwctx->log, 0, "proxy_wasm action "
                       "\"DONE\" -> \"CONTINUE\" to resume later phases");
        ngx_proxy_wasm_ctx_set_next_action(pwctx,
                                           NGX_PROXY_WASM_ACTION_CONTINUE);
        rc = NGX_DONE;
        goto done;

    case NGX_PROXY_WASM_ACTION_CONTINUE:
        goto cont;

    case NGX_PROXY_WASM_ACTION_PAUSE:
        switch (pwctx->phase->index) {
#ifdef NGX_WASM_HTTP
        case NGX_HTTP_WASM_HEADER_FILTER_PHASE:
            instance = ngx_proxy_wasm_pwexec2instance(pwexec);
            rctx = ngx_http_proxy_wasm_get_rctx(instance);

            ngx_log_debug3(NGX_LOG_DEBUG_WASM, pwctx->log, 0,
                           "proxy_wasm producing local response in "
                           "\"ResponseHeaders\" step "
                           "(filter: %l/%l, pwctx: %p)",
                           pwexec->index + 1, pwctx->nfilters, pwctx);

            if (!rctx->resp_chunk_override) {
                ngx_proxy_wasm_log_error(NGX_LOG_WARN, pwexec->log, 0,
                                         "\"ResponseHeaders\" returned "
                                         "\"PAUSE\": local response expected "
                                         "but none produced");
            }

            goto yield;

        case NGX_HTTP_WASM_BODY_FILTER_PHASE:
            instance = ngx_proxy_wasm_pwexec2instance(pwexec);
            rctx = ngx_http_proxy_wasm_get_rctx(instance);

            ngx_log_debug3(NGX_LOG_DEBUG_WASM, pwctx->log, 0,
                           "proxy_wasm buffering response after "
                           "\"ResponseBody\" step "
                           "(filter: %l/%l, pwctx: %p)",
                           pwexec->index + 1, pwctx->nfilters, pwctx);

            rctx->resp_buffering = 1;
            goto yield;

        case NGX_HTTP_REWRITE_PHASE:
        case NGX_HTTP_ACCESS_PHASE:
        case NGX_HTTP_CONTENT_PHASE:
        case NGX_WASM_BACKGROUND_PHASE:
            ngx_log_debug6(NGX_LOG_DEBUG_WASM, pwctx->log, 0,
                           "proxy_wasm pausing in \"%V\" phase "
                           "(filter: %l/%l, step: %d, action: %d, "
                           "pwctx: %p)", &pwctx->phase->name,
                           pwexec->index + 1, pwctx->nfilters,
                           pwctx->step, action, pwctx);
            goto yield;
#endif
        default:
            ngx_proxy_wasm_log_error(NGX_LOG_ERR, pwctx->log, pwexec->ecode,
                                     "bad \"%V\" return action: \"PAUSE\"",
                                     ngx_proxy_wasm_step_name(pwctx->step));

            pwexecs = (ngx_proxy_wasm_exec_t *) pwctx->pwexecs.elts;
            pwexec = &pwexecs[pwctx->exec_index];
            pwexec->ecode = NGX_PROXY_WASM_ERR_RETURN_ACTION;
            break;
        }

        break;

    default:
        ngx_proxy_wasm_log_error(NGX_LOG_WASM_NYI, pwctx->log, 0,
                                 "NYI - \"%V\" return action: %d",
                                 ngx_proxy_wasm_step_name(pwctx->step),
                                 action);
        goto error;

    }

error:

    ngx_wa_assert(rc == NGX_ERROR
#ifdef NGX_WASM_HTTP
                    || rc >= NGX_HTTP_SPECIAL_RESPONSE
#endif
                    );

    if (rc == NGX_ERROR && !pwexec->ecode) {
        pwexec->ecode = NGX_PROXY_WASM_ERR_UNKNOWN;
    }

    goto done;

#ifdef NGX_WASM_HTTP
yield:

    rc = NGX_AGAIN;
    goto done;
#endif

cont:

    rc = NGX_OK;

done:

    return rc;
}


ngx_int_t
ngx_proxy_wasm_resume(ngx_proxy_wasm_ctx_t *pwctx,
    ngx_wasm_phase_t *phase, ngx_proxy_wasm_step_e step)
{
    size_t                  i;
    ngx_int_t               rc = NGX_OK;
    ngx_proxy_wasm_exec_t  *pwexec, *pwexecs;

    dd("enter");

    pwctx->step = step;

    switch (step) {
    case NGX_PROXY_WASM_STEP_TICK:
    case NGX_PROXY_WASM_STEP_DONE:
    case NGX_PROXY_WASM_STEP_RESP_BODY:
    case NGX_PROXY_WASM_STEP_DISPATCH_RESPONSE:
        break;
    case NGX_PROXY_WASM_STEP_RESP_HEADERS:
        if (pwctx->last_completed_step < NGX_PROXY_WASM_STEP_RESP_HEADERS) {
            /* first execution of response phases, ensure the chain is reset */
            ngx_proxy_wasm_ctx_reset_chain(pwctx);
        }

        break;
    default:
        if (step <= pwctx->last_completed_step) {
            dd("step %d already completed, exit", step);
            ngx_wa_assert(rc == NGX_OK);
            goto ret;
        }
    }

    /* resume filters chain */

    pwexecs = (ngx_proxy_wasm_exec_t *) pwctx->pwexecs.elts;

    ngx_wa_assert(pwctx->pwexecs.nelts == pwctx->nfilters);

    for (i = pwctx->exec_index; i < pwctx->nfilters; i++) {
        pwexec = &pwexecs[i];

        dd("---> exec_index: %ld (%ld/%ld) \"%.*s\" with config: \"%.*s\", "
           "pwexec: %p, pwctx: %p",
           pwctx->exec_index,
           ngx_min(pwctx->exec_index + 1, pwctx->nfilters), pwctx->nfilters,
           (int) pwexec->filter->name->len, pwexec->filter->name->data,
           (int) pwexec->filter->config.len, pwexec->filter->config.data,
           pwexec, pwctx);

        ngx_wa_assert(pwexec->root_id != NGX_PROXY_WASM_ROOT_CTX_ID);

        /* check for yielded state */

        rc = action2rc(pwctx, pwexec);
        if (rc != NGX_OK) {
            goto ret;
        }

        if (step == NGX_PROXY_WASM_STEP_DONE
            && (pwexec->ictx == NULL || pwexec->ictx->instance->trapped))
        {
            dd("no instance, skip done");
            rc = NGX_OK;
            goto ret;
        }

        /* run step */

        pwexec->ecode = ngx_proxy_wasm_run_step(pwexec, step);
        dd("<--- run_step ecode: %d, pwctx->action: %d",
           pwexec->ecode, pwctx->action);
        if (pwexec->ecode != NGX_PROXY_WASM_ERR_NONE) {
            rc = pwexec->filter->subsystem->ecode(pwexec->ecode);
            goto ret;
        }

        switch (pwctx->action) {
        case NGX_PROXY_WASM_ACTION_CONTINUE:
            dd("-------- next filter --------");
            pwctx->exec_index++;
            break;
        case NGX_PROXY_WASM_ACTION_PAUSE:
            /**
             * Exception for response body buffering which re-enters
             * the same filter once the response is buffered.
             */
            if (step != NGX_PROXY_WASM_STEP_RESP_BODY) {
                dd("-------- pause --------");
                pwctx->exec_index++;
            }

            /* fallthrough */

        default:
            break;
        }

        /* check for yield/done */

        rc = action2rc(pwctx, pwexec);
        if (rc != NGX_OK && rc != NGX_AGAIN) {
            goto ret;
        }
    }

    /* next step */

    pwctx->last_completed_step = pwctx->step;
    pwctx->exec_index = 0;

ret:

    if (step == NGX_PROXY_WASM_STEP_DONE) {
        ngx_proxy_wasm_ctx_destroy(pwctx);
    }

    dd("exit rc: %ld", rc);

    return rc;
}


ngx_proxy_wasm_err_e
ngx_proxy_wasm_run_step(ngx_proxy_wasm_exec_t *pwexec,
    ngx_proxy_wasm_step_e step)
{
    ngx_int_t                 rc;
    ngx_proxy_wasm_err_e      ecode;
    ngx_proxy_wasm_exec_t    *out;
    ngx_proxy_wasm_ctx_t     *pwctx = pwexec->parent;
    ngx_proxy_wasm_filter_t  *filter = pwexec->filter;
    ngx_proxy_wasm_action_e   old_action = pwctx->action;
    ngx_proxy_wasm_action_e   action = old_action;

    ngx_wa_assert(pwctx->phase);

    dd("--> enter (pwexec: %p, ictx: %p, trapped: %d)",
       pwexec, pwexec->ictx,
       pwexec->ictx ? pwexec->ictx->instance->trapped : 0);

#if 1
    /* optimization to avoid ctx lookups  */
    if (pwexec->ictx == NULL || pwexec->ictx->instance->trapped) {
#endif
        ecode = ngx_proxy_wasm_create_context(filter, pwctx, pwexec->id,
                                              pwexec, &out);
        if (ecode != NGX_PROXY_WASM_ERR_NONE) {
            return ecode;
        }

        pwexec = out;
#if 1
    }
#endif

    ngx_wa_assert(!pwexec->ictx->instance->trapped);
    ngx_wa_assert(pwexec->ictx->module == filter->module);

    pwctx->step = step;

    ngx_proxy_wasm_instance_update(pwexec->ictx, pwexec);

    if (pwexec->root_id == NGX_PROXY_WASM_ROOT_CTX_ID) {
        ngx_proxy_wasm_log_error(NGX_LOG_DEBUG, pwexec->log, 0,
                                 "root context resuming \"%V\" step "
                                 "in \"%V\" phase",
                                 ngx_proxy_wasm_step_name(step),
                                 &pwctx->phase->name);

    } else {
        ngx_proxy_wasm_log_error(NGX_LOG_DEBUG, pwexec->log, 0,
                                 "filter %l/%l resuming \"%V\" step "
                                 "in \"%V\" phase",
                                 pwexec->index + 1, pwctx->nfilters,
                                 ngx_proxy_wasm_step_name(step),
                                 &pwctx->phase->name);
    }

    switch (step) {
    case NGX_PROXY_WASM_STEP_REQ_HEADERS:
    case NGX_PROXY_WASM_STEP_REQ_BODY:
    case NGX_PROXY_WASM_STEP_RESP_HEADERS:
    case NGX_PROXY_WASM_STEP_RESP_BODY:
#ifdef NGX_WASM_RESPONSE_TRAILERS
    case NGX_PROXY_WASM_STEP_RESP_TRAILERS:
#endif
        rc = filter->subsystem->resume(pwexec, step, &action);
        break;
    case NGX_PROXY_WASM_STEP_LOG:
        ngx_proxy_wasm_on_log(pwexec);
        rc = NGX_OK;
        break;
    case NGX_PROXY_WASM_STEP_DONE:
        ngx_proxy_wasm_on_done(pwexec);
        rc = NGX_OK;
        break;
    case NGX_PROXY_WASM_STEP_DISPATCH_RESPONSE:
        rc = filter->subsystem->resume(pwexec, step, &action);
        break;
    case NGX_PROXY_WASM_STEP_TICK:
        pwexec->in_tick = 1;
        rc = ngx_proxy_wasm_on_tick(pwexec);
        pwexec->in_tick = 0;
        break;
    default:
        ngx_proxy_wasm_log_error(NGX_LOG_WASM_NYI, pwctx->log, 0,
                                 "NYI - proxy_wasm step: %d", step);
        rc = NGX_ERROR;
        break;
    }

    dd("<-- step rc: %ld, old_action: %d, ret action: %d, pwctx->action: %d, "
       " ictx: %p", rc, old_action, action, pwctx->action, pwexec->ictx);

    /* pwctx->action writes in host calls overwrite action return value */

    if (pwctx->action != action) {
        switch (pwctx->action) {
        case NGX_PROXY_WASM_ACTION_DONE:
            /* e.g. proxy_send_local_response() */
            ngx_wa_assert(pwctx->action != old_action);
            goto done;
        default:
            ngx_proxy_wasm_ctx_set_next_action(pwctx, action);
            break;
        }
    }

    ngx_wa_assert(rc == NGX_OK
                  || rc == NGX_ABORT
                  || rc == NGX_ERROR);

    if (rc == NGX_OK) {
        pwexec->ecode = NGX_PROXY_WASM_ERR_NONE;

    } else if (rc == NGX_ABORT) {
        pwexec->ecode = NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED;

    } else if (rc == NGX_ERROR) {
        pwexec->ecode = NGX_PROXY_WASM_ERR_UNKNOWN;
    }

done:

    return pwexec->ecode;
}


ngx_uint_t
ngx_proxy_wasm_dispatch_calls_total(ngx_proxy_wasm_exec_t *pwexec)
{
    ngx_queue_t  *q;
    ngx_uint_t    n = 0;

    for (q = ngx_queue_head(&pwexec->calls);
         q != ngx_queue_sentinel(&pwexec->calls);
         q = ngx_queue_next(q), n++) { /* void */ }

    return n;
}


void
ngx_proxy_wasm_dispatch_calls_cancel(ngx_proxy_wasm_exec_t *pwexec)
{
#ifdef NGX_WASM_HTTP
    ngx_queue_t                     *q;
    ngx_http_proxy_wasm_dispatch_t  *call;

    while (!ngx_queue_empty(&pwexec->calls)) {
        q = ngx_queue_head(&pwexec->calls);
        call = ngx_queue_data(q, ngx_http_proxy_wasm_dispatch_t, q);

        ngx_log_debug1(NGX_LOG_DEBUG_ALL, pwexec->log, 0,
                       "proxy_wasm http dispatch cancelled (dispatch: %p)",
                       call);

        ngx_queue_remove(&call->q);

        ngx_http_proxy_wasm_dispatch_destroy(call);
    }
#endif
}


/* host handlers */


ngx_wavm_ptr_t
ngx_proxy_wasm_alloc(ngx_proxy_wasm_exec_t *pwexec, size_t size)
{
    ngx_wavm_ptr_t            p;
    ngx_int_t                 rc;
    wasm_val_vec_t           *rets;
    ngx_proxy_wasm_filter_t  *filter = pwexec->filter;
    ngx_wavm_instance_t      *instance = ngx_proxy_wasm_pwexec2instance(pwexec);

    rc = ngx_wavm_instance_call_funcref(instance,
                                        filter->proxy_on_memory_allocate,
                                        &rets, size);
    if (rc != NGX_OK) {
        ngx_proxy_wasm_log_error(NGX_LOG_CRIT, pwexec->log, 0,
                                 "proxy_wasm_alloc(%uz) failed", size);
        return 0;
    }

    p = rets->data[0].of.i32;

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, pwexec->log, 0,
                   "proxy_wasm_alloc: %uz:%uz:%uz",
                   ngx_wavm_memory_data_size(instance->memory), p, size);

    return p;
}


static ngx_proxy_wasm_instance_t *
get_instance(ngx_proxy_wasm_filter_t *filter,
    ngx_proxy_wasm_store_t *store, ngx_log_t *log)
{
    ngx_queue_t                *q;
    ngx_wavm_module_t          *module = filter->module;
    ngx_proxy_wasm_instance_t  *ictx;

    dd("get instance in store: %p", store);

    /* store initialized */
    ngx_wa_assert(store->pool);

    for (q = ngx_queue_head(&store->busy);
         q != ngx_queue_sentinel(&store->busy);
         q = ngx_queue_next(q))
    {
        ictx = ngx_queue_data(q, ngx_proxy_wasm_instance_t, q);

        if (ictx->instance->trapped) {
            q = ngx_queue_next(&ictx->q);
            ngx_proxy_wasm_log_error(NGX_LOG_DEBUG, log, 0,
                                     "\"%V\" filter invalidating trapped "
                                     "instance (ictx: %p, store: %p)",
                                     filter->name, ictx, store);
            ngx_proxy_wasm_instance_invalidate(ictx);
            continue;
        }

        if (ictx->module == module) {
            dd("reuse busy instance");
            goto reuse;
        }
    }

#if 0
    for (q = ngx_queue_head(&store->free);
         q != ngx_queue_sentinel(&store->free);
         q = ngx_queue_next(q))
    {
        ictx = ngx_queue_data(q, ngx_proxy_wasm_instance_t, q);

        ngx_wa_assert(!ictx->instance->trapped);

        if (ictx->module == module) {
            dd("reuse free instance, going to busy");
            ngx_queue_remove(&ictx->q);
            ngx_queue_insert_tail(&store->busy, &ictx->q);
            goto reuse;
        }
    }
#endif

    dd("create instance in store: %p", store);

    ictx = ngx_pcalloc(store->pool, sizeof(ngx_proxy_wasm_instance_t));
    if (ictx == NULL) {
        goto error;
    }

    ictx->pool = store->pool;
    ictx->log = log;
    ictx->store = store;
    ictx->module = module;

    ngx_rbtree_init(&ictx->root_ctxs, &ictx->sentinel_root_ctxs,
                    ngx_rbtree_insert_value);

    ngx_rbtree_init(&ictx->tree_ctxs, &ictx->sentinel_ctxs,
                    ngx_rbtree_insert_value);

    ictx->instance = ngx_wavm_instance_create(ictx->module, ictx->pool,
                                              ictx->log, ictx);
    if (ictx->instance == NULL) {
        ngx_pfree(store->pool, ictx);
        goto error;
    }

    ngx_proxy_wasm_log_error(NGX_LOG_DEBUG, log, 0,
                             "\"%V\" filter new instance (ictx: %p, store: %p)",
                             filter->name, ictx, store);

    ngx_queue_insert_tail(&store->busy, &ictx->q);

    goto done;

reuse:

    ngx_proxy_wasm_log_error(NGX_LOG_DEBUG, log, 0,
                             "\"%V\" filter reusing instance "
                             "(ictx: %p, store: %p)",
                             filter->name, ictx, store);

done:

    return ictx;

error:

    return NULL;
}


static ngx_proxy_wasm_err_e
ngx_proxy_wasm_create_context(ngx_proxy_wasm_filter_t *filter,
    ngx_proxy_wasm_ctx_t *pwctx, ngx_uint_t id, ngx_proxy_wasm_exec_t *in,
    ngx_proxy_wasm_exec_t **out)
{
    ngx_int_t                   rc;
    ngx_log_t                  *log;
    wasm_val_vec_t             *rets;
    ngx_proxy_wasm_instance_t  *ictx;
    ngx_proxy_wasm_store_t     *store;
    ngx_proxy_wasm_exec_t      *rexec = NULL, *pwexec = NULL;
    ngx_proxy_wasm_err_e        ecode = NGX_PROXY_WASM_ERR_UNKNOWN;

    dd("enter (filter: \"%.*s\", id: %ld)",
       (int) filter->name->len, filter->name->data, id);

    if (id == NGX_PROXY_WASM_ROOT_CTX_ID
        || (in && in->root_id == NGX_PROXY_WASM_ROOT_CTX_ID))
    {
        /* root context */
        log = filter->log;
        store = filter->store;

        /* sweep if an instance has trapped */
        ngx_proxy_wasm_store_sweep(store);

    } else {
        /* filter context */
        log = pwctx->log;

        switch (pwctx->isolation) {
        case NGX_PROXY_WASM_ISOLATION_NONE:
            store = filter->store;
            break;
        case NGX_PROXY_WASM_ISOLATION_STREAM:
            store = &pwctx->store;
            break;
        case NGX_PROXY_WASM_ISOLATION_FILTER:
            store = ngx_palloc(pwctx->pool, sizeof(ngx_proxy_wasm_store_t));
            if (store == NULL) {
                goto error;
            }

            ngx_proxy_wasm_store_init(store, pwctx->pool);
            break;
        default:
            ngx_proxy_wasm_log_error(NGX_LOG_WASM_NYI, pwctx->log, 0,
                                     "NYI - instance isolation: %d",
                                     pwctx->isolation);
            goto error;
        }
    }

    /* get instance */

    if (in && in->ictx && !in->ictx->instance->trapped) {
        ictx = in->ictx;

    } else {
        ictx = get_instance(filter, store, log);
        if (ictx == NULL) {
            goto error;
        }
    }

    /* create root context */

    rexec = ngx_proxy_wasm_lookup_root_ctx(ictx, filter->id);
    dd("rexec for id %ld: %p (in: %p)", filter->id, rexec, in);
    if (rexec == NULL) {
        if (in == NULL || (in && in->root_id != NGX_PROXY_WASM_ROOT_CTX_ID)) {
            rexec = ngx_pcalloc(filter->pool, sizeof(ngx_proxy_wasm_exec_t));
            if (rexec == NULL) {
                ecode = NGX_PROXY_WASM_ERR_START_FAILED;
                goto error;
            }

            rexec->root_id = NGX_PROXY_WASM_ROOT_CTX_ID;
            rexec->id = filter->id;
            rexec->pool = filter->pool;
            rexec->log = filter->log;
            rexec->filter = filter;
            rexec->ictx = ictx;

            ngx_queue_init(&rexec->calls);

            log = filter->log;

            rexec->log = ngx_pcalloc(rexec->pool, sizeof(ngx_log_t));
            if (rexec->log == NULL) {
                ecode = NGX_PROXY_WASM_ERR_START_FAILED;
                goto error;
            }

            rexec->log->file = log->file;
            rexec->log->next = log->next;
            rexec->log->writer = log->writer;
            rexec->log->wdata = log->wdata;
            rexec->log->log_level = log->log_level;
            rexec->log->handler = ngx_proxy_wasm_log_error_handler;
            rexec->log->data = &rexec->log_ctx;

            rexec->log_ctx.pwexec = rexec;
            rexec->log_ctx.orig_log = log;

            rexec->parent = ngx_pcalloc(rexec->pool,
                                        sizeof(ngx_proxy_wasm_ctx_t));
            if (rexec->parent == NULL) {
                ecode = NGX_PROXY_WASM_ERR_START_FAILED;
                goto error;
            }

            rexec->parent->id = NGX_PROXY_WASM_ROOT_CTX_ID;
            rexec->parent->pool = rexec->pool;
            rexec->parent->log = rexec->log;
            rexec->parent->isolation = NGX_PROXY_WASM_ISOLATION_STREAM;

        } else {
            if (in->ictx != ictx) {
                dd("replace pwexec instance");
                ngx_wa_assert(in->root_id == NGX_PROXY_WASM_ROOT_CTX_ID);

                in->ictx = ictx;
                in->started = 0;
                in->tick_period = 0;
            }

            rexec = in;
        }
    }

    /* start root context */

    if (!rexec->started) {
        dd("start root exec ctx (rexec: %p, root_id: %ld, id: %ld, ictx: %p)",
           rexec, rexec->root_id, rexec->id, ictx);

        ngx_proxy_wasm_instance_update(ictx, rexec);

        rc = ngx_wavm_instance_call_funcref(ictx->instance,
                                            filter->proxy_on_context_create,
                                            NULL,
                                            rexec->id, rexec->root_id);
        if (rc != NGX_OK) {
            ecode = NGX_PROXY_WASM_ERR_START_FAILED;
            goto error;
        }

        if (id == NGX_PROXY_WASM_ROOT_CTX_ID) {
            rc = ngx_wavm_instance_call_funcref(ictx->instance,
                                                filter->proxy_on_vm_start,
                                                &rets,
                                                rexec->id, rexec->root_id);
            if (rc != NGX_OK || !rets->data[0].of.i32) {
                ecode = NGX_PROXY_WASM_ERR_VM_START_FAILED;
                goto error;
            }
        }

        rc = ngx_wavm_instance_call_funcref(ictx->instance,
                                            filter->proxy_on_plugin_start,
                                            &rets,
                                            rexec->id, filter->config.len);
        if (rc != NGX_OK || !rets->data[0].of.i32) {
            ecode = NGX_PROXY_WASM_ERR_CONFIGURE_FAILED;
            goto error;
        }

        dd("link root ctx #%ld instance (rexec: %p)", rexec->id, rexec);

        rexec->node.key = rexec->id;
        ngx_rbtree_insert(&ictx->root_ctxs, &rexec->node);

        rexec->started = 1;
    }

    /* start filter context */

    if (id && (in == NULL || in->root_id != NGX_PROXY_WASM_ROOT_CTX_ID)) {
        pwexec = ngx_proxy_wasm_lookup_ctx(ictx, id);
        dd("pwexec for id %ld: %p (in: %p)", id, pwexec, in);
        if (pwexec == NULL) {

            if (in == NULL) {
                pwexec = ngx_array_push(&pwctx->pwexecs);
                if (pwexec == NULL) {
                    ecode = NGX_PROXY_WASM_ERR_START_FAILED;
                    goto error;
                }

                ngx_memzero(pwexec, sizeof(ngx_proxy_wasm_exec_t));

                pwexec->id = id;
                pwexec->root_id = filter->id;
                pwexec->index = pwctx->pwexecs.nelts - 1;
                pwexec->pool = pwctx->pool;
                pwexec->filter = filter;
                pwexec->parent = pwctx;
                pwexec->ictx = ictx;
                pwexec->store = ictx->store;

                ngx_queue_init(&pwexec->calls);

            } else {
                if (in->ictx != ictx) {
                    dd("replace pwexec instance");
                    in->ictx = ictx;
                    in->started = 0;
                }

                pwexec = in;
            }

            if (pwexec->log == NULL) {
                log = pwctx->log;

                pwexec->log = ngx_pcalloc(pwexec->pool, sizeof(ngx_log_t));
                if (pwexec->log == NULL) {
                    ecode = NGX_PROXY_WASM_ERR_START_FAILED;
                    goto error;
                }

                pwexec->log->file = log->file;
                pwexec->log->next = log->next;
                pwexec->log->writer = log->writer;
                pwexec->log->wdata = log->wdata;
                pwexec->log->log_level = log->log_level;
                pwexec->log->connection = log->connection;
                pwexec->log->handler = ngx_proxy_wasm_log_error_handler;
                pwexec->log->data = &pwexec->log_ctx;

                pwexec->log_ctx.pwexec = pwexec;
                pwexec->log_ctx.orig_log = log;
            }
        }
#if (NGX_DEBUG)
        else {
            ngx_wa_assert(pwexec->id == id);
            dd("pwexec #%ld found in instance %p", pwexec->id, ictx);
        }
#endif

        if (!pwexec->started) {
            dd("start exec ctx (pwexec: %p, id: %ld, ictx: %p)",
               pwexec, id, ictx);

            ngx_proxy_wasm_instance_update(ictx, pwexec);

            rc = ngx_wavm_instance_call_funcref(ictx->instance,
                                                filter->proxy_on_context_create,
                                                NULL,
                                                id, filter->id);
            if (rc != NGX_OK) {
                ecode = NGX_PROXY_WASM_ERR_START_FAILED;
                goto error;
            }

            pwexec->node.key = pwexec->id;
            ngx_rbtree_insert(&ictx->tree_ctxs, &pwexec->node);

            pwexec->started = 1;
        }

        if (out) {
            *out = pwexec;
        }

    } else {
        if (out) {
            *out = rexec;
        }
    }

    return NGX_PROXY_WASM_ERR_NONE;

error:

    if (ecode != NGX_PROXY_WASM_ERR_NONE) {
        if (pwexec) {
            dd("set ecode to %d", ecode);
            pwexec->ecode = ecode;

        } else {
            dd("set filter ecode to %d", ecode);
            filter->ecode = ecode;
        }
    }

    return ecode;
}


static void
ngx_proxy_wasm_on_log(ngx_proxy_wasm_exec_t *pwexec)
{
    ngx_proxy_wasm_filter_t  *filter = pwexec->filter;
    ngx_wavm_instance_t      *instance = ngx_proxy_wasm_pwexec2instance(pwexec);

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        (void) ngx_wavm_instance_call_funcref(instance, filter->proxy_on_done,
                                              NULL, pwexec->id);
    }

    (void) ngx_wavm_instance_call_funcref(instance, filter->proxy_on_log,
                                          NULL, pwexec->id);
}


static void
ngx_proxy_wasm_on_done(ngx_proxy_wasm_exec_t *pwexec)
{
    ngx_wavm_instance_t             *instance;
    ngx_proxy_wasm_filter_t         *filter = pwexec->filter;
#if 0
#ifdef NGX_WASM_HTTP
    ngx_http_proxy_wasm_dispatch_t  *call;
#endif
#endif

    instance = ngx_proxy_wasm_pwexec2instance(pwexec);

    ngx_proxy_wasm_log_error(NGX_LOG_DEBUG, pwexec->log, 0,
                             "filter %l/%l finalizing context",
                             pwexec->index + 1, pwexec->parent->nfilters);

#if 0
#ifdef NGX_WASM_HTTP
    call = pwexec->call;
    if (call) {
        ngx_http_proxy_wasm_dispatch_destroy(call);

        pwexec->call = NULL;
    }
#endif
#endif

    (void) ngx_wavm_instance_call_funcref(instance,
                                          filter->proxy_on_context_finalize,
                                          NULL, pwexec->id);

    if (pwexec->node.key) {
        ngx_rbtree_delete(&pwexec->ictx->tree_ctxs, &pwexec->node);
    }
}


static ngx_int_t
ngx_proxy_wasm_on_tick(ngx_proxy_wasm_exec_t *pwexec)
{
    ngx_int_t                 rc;
    wasm_val_vec_t            args;
    ngx_proxy_wasm_filter_t  *filter = pwexec->filter;

    ngx_wa_assert(pwexec->root_id == NGX_PROXY_WASM_ROOT_CTX_ID);

    wasm_val_vec_new_uninitialized(&args, 1);
    ngx_wasm_vec_set_i32(&args, 0, pwexec->id);

    rc = ngx_wavm_instance_call_funcref_vec(pwexec->ictx->instance,
                                            filter->proxy_on_timer_ready,
                                            NULL, &args);

    wasm_val_vec_delete(&args);

    return rc;
}


/* utils */


static ngx_proxy_wasm_filter_t *
ngx_proxy_wasm_lookup_filter(ngx_proxy_wasm_filters_root_t *pwroot,
    ngx_uint_t id)
{
    ngx_rbtree_t             *rbtree;
    ngx_rbtree_node_t        *node, *sentinel;
    ngx_proxy_wasm_filter_t  *filter;

    rbtree = &pwroot->tree;
    node = rbtree->root;
    sentinel = rbtree->sentinel;

    while (node != sentinel) {

        if (id != node->key) {
            node = (id < node->key) ? node->left : node->right;
            continue;
        }

        filter = ngx_rbtree_data(node, ngx_proxy_wasm_filter_t, node);

        return filter;
    }

    return NULL;
}


static ngx_proxy_wasm_exec_t *
ngx_proxy_wasm_lookup_root_ctx(ngx_proxy_wasm_instance_t *ictx, ngx_uint_t id)
{
    ngx_rbtree_t           *rbtree;
    ngx_rbtree_node_t      *node, *sentinel;
    ngx_proxy_wasm_exec_t  *pwexec;

    rbtree = &ictx->root_ctxs;
    node = rbtree->root;
    sentinel = rbtree->sentinel;

    while (node != sentinel) {

        if (id != node->key) {
            node = (id < node->key) ? node->left : node->right;
            continue;
        }

        pwexec = ngx_rbtree_data(node, ngx_proxy_wasm_exec_t, node);

        return pwexec;
    }

    return NULL;
}


static ngx_proxy_wasm_exec_t *
ngx_proxy_wasm_lookup_ctx(ngx_proxy_wasm_instance_t *ictx, ngx_uint_t id)
{
    ngx_rbtree_t           *rbtree;
    ngx_rbtree_node_t      *node, *sentinel;
    ngx_proxy_wasm_exec_t  *pwexec;

    rbtree = &ictx->tree_ctxs;
    node = rbtree->root;
    sentinel = rbtree->sentinel;

    while (node != sentinel) {

        if (id != node->key) {
            node = (id < node->key) ? node->left : node->right;
            continue;
        }

        pwexec = ngx_rbtree_data(node, ngx_proxy_wasm_exec_t, node);

        return pwexec;
    }

    return NULL;
}


static ngx_wavm_funcref_t *
get_func(ngx_proxy_wasm_filter_t *filter, const char *n)
{
    ngx_str_t   name;

    name.data = (u_char *) n;
    name.len = ngx_strlen(n);

    return ngx_wavm_module_func_lookup(filter->module, &name);
}


static ngx_proxy_wasm_abi_version_e
get_abi_version(ngx_proxy_wasm_filter_t *filter)
{
    size_t                    i;
    ngx_wavm_module_t        *module = filter->module;
    const wasm_exporttype_t  *exporttype;
    const wasm_name_t        *exportname;

    for (i = 0; i < module->exports.size; i++) {
        exporttype = ((wasm_exporttype_t **) module->exports.data)[i];
        exportname = wasm_exporttype_name(exporttype);

        if (ngx_str_eq(exportname->data, exportname->size,
                       "proxy_abi_version_0_2_1", -1))
        {
            return NGX_PROXY_WASM_0_2_1;
        }

        if (ngx_str_eq(exportname->data, exportname->size,
                       "proxy_abi_version_0_2_0", -1))
        {
            return NGX_PROXY_WASM_0_2_0;
        }

        if (ngx_str_eq(exportname->data, exportname->size,
                       "proxy_abi_version_0_1_0", -1))
        {
            return NGX_PROXY_WASM_0_1_0;
        }

#if (NGX_DEBUG)
        if (ngx_str_eq(exportname->data, exportname->size,
                       "proxy_abi_version_vnext", -1))
        {
            return NGX_PROXY_WASM_VNEXT;
        }
#endif
    }

    return NGX_PROXY_WASM_UNKNOWN;
}


static ngx_int_t
ngx_proxy_wasm_filter_init_abi(ngx_proxy_wasm_filter_t *filter)
{
    ngx_log_debug4(NGX_LOG_DEBUG_WASM, filter->log, 0,
                   "proxy_wasm initializing \"%V\" filter "
                   "(config size: %d, filter: %p, filter->id: %ui)",
                   filter->name, filter->config.len, filter, filter->id);

    filter->abi_version = get_abi_version(filter);

    switch (filter->abi_version) {
    case NGX_PROXY_WASM_0_1_0:
    case NGX_PROXY_WASM_0_2_0:
    case NGX_PROXY_WASM_0_2_1:
        break;
    case NGX_PROXY_WASM_UNKNOWN:
        filter->ecode = NGX_PROXY_WASM_ERR_UNKNOWN_ABI;
        break;
    default:
        filter->ecode = NGX_PROXY_WASM_ERR_BAD_ABI;
        break;
    }

    if (filter->ecode) {
        ngx_proxy_wasm_log_error(NGX_LOG_EMERG, filter->log, filter->ecode,
                                 "failed loading \"%V\" filter",
                                 filter->name);
        return NGX_ERROR;
    }

    /* malloc */

    filter->proxy_on_memory_allocate = get_func(filter, "malloc");

    if (filter->proxy_on_memory_allocate == NULL) {
        filter->proxy_on_memory_allocate =
            get_func(filter, "proxy_on_memory_allocate");

        if (filter->proxy_on_memory_allocate == NULL) {
            filter->ecode = NGX_PROXY_WASM_ERR_BAD_MODULE_INTERFACE;

            ngx_proxy_wasm_log_error(NGX_LOG_EMERG, filter->log, filter->ecode,
                                     "\"%V\" filter missing malloc",
                                     filter->name);
            return NGX_ERROR;
        }
    }

    /* context */

    filter->proxy_on_context_create =
        get_func(filter, "proxy_on_context_create");
    filter->proxy_on_context_finalize =
        get_func(filter, "proxy_on_context_finalize");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_done =
            get_func(filter, "proxy_on_done");
        filter->proxy_on_log =
            get_func(filter, "proxy_on_log");
        filter->proxy_on_context_finalize =
            get_func(filter, "proxy_on_delete");
    }

    /* configuration */

    filter->proxy_on_vm_start =
        get_func(filter, "proxy_on_vm_start");
    filter->proxy_on_plugin_start =
        get_func(filter, "proxy_on_plugin_start");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_plugin_start =
            get_func(filter, "proxy_on_configure");
    }

    /* stream */

    filter->proxy_on_new_connection =
        get_func(filter, "proxy_on_new_connection");
    filter->proxy_on_downstream_data =
        get_func(filter, "proxy_on_downstream_data");
    filter->proxy_on_upstream_data =
        get_func(filter, "proxy_on_upstream_data");
    filter->proxy_on_downstream_close =
        get_func(filter, "proxy_on_downstream_close");
    filter->proxy_on_upstream_close =
        get_func(filter, "proxy_on_upstream_close");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_downstream_close =
            get_func(filter, "proxy_on_downstream_connection_close");
        filter->proxy_on_upstream_close =
            get_func(filter, "proxy_on_upstream_connection_close");
    }

    /* http */

    filter->proxy_on_http_request_headers =
        get_func(filter, "proxy_on_http_request_headers");
    filter->proxy_on_http_request_body =
        get_func(filter, "proxy_on_http_request_body");
    filter->proxy_on_http_request_trailers =
        get_func(filter, "proxy_on_http_request_trailers");
    filter->proxy_on_http_request_metadata =
        get_func(filter, "proxy_on_http_request_metadata");
    filter->proxy_on_http_response_headers =
        get_func(filter, "proxy_on_http_response_headers");
    filter->proxy_on_http_response_body =
        get_func(filter, "proxy_on_http_response_body");
    filter->proxy_on_http_response_trailers =
        get_func(filter, "proxy_on_http_response_trailers");
    filter->proxy_on_http_response_metadata =
        get_func(filter, "proxy_on_http_response_metadata");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_http_request_headers =
            get_func(filter, "proxy_on_request_headers");
        filter->proxy_on_http_request_body =
            get_func(filter, "proxy_on_request_body");
        filter->proxy_on_http_request_trailers =
            get_func(filter, "proxy_on_request_trailers");
        filter->proxy_on_http_request_metadata =
            get_func(filter, "proxy_on_request_metadata");
        filter->proxy_on_http_response_headers =
            get_func(filter, "proxy_on_response_headers");
        filter->proxy_on_http_response_body =
            get_func(filter, "proxy_on_response_body");
        filter->proxy_on_http_response_trailers =
            get_func(filter, "proxy_on_response_trailers");
        filter->proxy_on_http_response_metadata =
            get_func(filter, "proxy_on_response_metadata");
    }

    /* shared queue */

    filter->proxy_on_queue_ready =
        get_func(filter, "proxy_on_queue_ready");

    /* timers */

    filter->proxy_create_timer =
        get_func(filter, "proxy_create_timer");
    filter->proxy_delete_timer =
        get_func(filter, "proxy_delete_timer");
    filter->proxy_on_timer_ready =
        get_func(filter, "proxy_on_timer_ready");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.1.0 - 0.2.1 */
        filter->proxy_on_timer_ready =
            get_func(filter, "proxy_on_tick");
    }

    /* http callouts */

    filter->proxy_on_http_call_response =
        get_func(filter, "proxy_on_http_call_response");

    /* grpc callouts */

    filter->proxy_on_grpc_call_response_header_metadata =
        get_func(filter, "proxy_on_grpc_call_response_header_metadata");
    filter->proxy_on_grpc_call_response_message =
        get_func(filter, "proxy_on_grpc_call_response_message");
    filter->proxy_on_grpc_call_response_trailer_metadata =
        get_func(filter, "proxy_on_grpc_call_response_trailer_metadata");
    filter->proxy_on_grpc_call_close =
        get_func(filter, "proxy_on_grpc_call_close");

    /* custom extensions */

    filter->proxy_on_custom_callback =
        get_func(filter, "proxy_on_custom_callback");

    if (filter->abi_version < NGX_PROXY_WASM_VNEXT) {
        /* 0.2.0 - 0.2.1 */
        filter->proxy_on_custom_callback =
            get_func(filter, "proxy_on_foreign_function");
    }

    /* validate */

    if (filter->proxy_on_context_create == NULL
        || filter->proxy_on_vm_start == NULL
        || filter->proxy_on_plugin_start == NULL)
    {
        filter->ecode = NGX_PROXY_WASM_ERR_BAD_MODULE_INTERFACE;

        ngx_proxy_wasm_log_error(NGX_LOG_EMERG, filter->log, filter->ecode,
                                 "\"%V\" filter missing one of: "
                                 "on_context_create, on_vm_start, "
                                 "on_plugin_start", filter->name);
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_proxy_wasm_filter_start(ngx_proxy_wasm_filter_t *filter)
{
    ngx_proxy_wasm_err_e   ecode;

    ngx_wa_assert(filter->loaded);

    if (filter->ecode) {
        return NGX_ERROR;
    }

    if (filter->started) {
        return NGX_OK;
    }

    ecode = ngx_proxy_wasm_create_context(filter, NULL, 0, NULL, NULL);
    if (ecode != NGX_PROXY_WASM_ERR_NONE) {
        return NGX_ERROR;
    }

    filter->started = 1;

    return NGX_OK;
}


static void
ngx_proxy_wasm_instance_update(ngx_proxy_wasm_instance_t *ictx,
    ngx_proxy_wasm_exec_t *pwexec)
{
    /**
     * Update instance
     * FFI-injected filters have a valid log while the instance's
     * might be outdated.
     */
    ictx->pwexec = pwexec;

    if (pwexec) {
        ngx_wavm_instance_set_data(ictx->instance, ictx, pwexec->log);
    }
}


static void
ngx_proxy_wasm_instance_invalidate(ngx_proxy_wasm_instance_t *ictx)
{
    ngx_rbtree_node_t      **root, **sentinel, *s, *n;
    ngx_proxy_wasm_exec_t   *pwexec;

    dd("enter (ictx: %p)", ictx);

    /* root context */

    root = &ictx->root_ctxs.root;
    s = &ictx->sentinel_root_ctxs;
    sentinel = &s;

    while (*root != *sentinel) {
        n = ngx_rbtree_min(*root, *sentinel);
        pwexec = ngx_rbtree_data(n, ngx_proxy_wasm_exec_t, node);

        dd("unlink instance of root ctx #%ld (rexec: %p)", pwexec->id, pwexec);

        pwexec->ictx = NULL;
        ngx_rbtree_delete(&ictx->root_ctxs, n);
    }

    /* stream context */

    root = &ictx->tree_ctxs.root;
    s = &ictx->sentinel_ctxs;
    sentinel = &s;

    while (*root != *sentinel) {
        n = ngx_rbtree_min(*root, *sentinel);
        pwexec = ngx_rbtree_data(n, ngx_proxy_wasm_exec_t, node);

        dd("destroy ctx #%ld (pwexec: %p)", pwexec->id, pwexec);

        ngx_rbtree_delete(&ictx->tree_ctxs, n);
        destroy_pwexec(pwexec);
    }

    /* remove from busy/free, schedule for sweeping */

    ngx_queue_remove(&ictx->q);

    ngx_queue_insert_tail(&ictx->store->sweep, &ictx->q);

    dd("exit");
}


static void
ngx_proxy_wasm_instance_destroy(ngx_proxy_wasm_instance_t *ictx)
{
    ngx_rbtree_node_t      **root, **sentinel, *s, *n;
    ngx_proxy_wasm_exec_t   *pwexec;

    dd("enter (ictx: %p)", ictx);

    /* root context */

    root = &ictx->root_ctxs.root;
    s = &ictx->sentinel_root_ctxs;
    sentinel = &s;

    while (*root != *sentinel) {
        n = ngx_rbtree_min(*root, *sentinel);
        pwexec = ngx_rbtree_data(n, ngx_proxy_wasm_exec_t, node);

        dd("unlink+destroy instance of root ctx #%ld (rexec: %p)",
           pwexec->id, pwexec);

        pwexec->ictx = NULL;
        ngx_rbtree_delete(&ictx->root_ctxs, n);
    }

    /* stream context */

    root = &ictx->tree_ctxs.root;
    s = &ictx->sentinel_ctxs;
    sentinel = &s;

    while (*root != *sentinel) {
        n = ngx_rbtree_min(*root, *sentinel);
        pwexec = ngx_rbtree_data(n, ngx_proxy_wasm_exec_t, node);

        ngx_wa_assert(pwexec->ev == NULL);

        dd("destroy ctx #%ld (pwexec: %p)", pwexec->id, pwexec);

        ngx_rbtree_delete(&ictx->tree_ctxs, n);
        destroy_pwexec(pwexec);
    }

    ngx_wavm_instance_destroy(ictx->instance);

    ngx_pfree(ictx->pool, ictx);

    dd("exit");
}


static void
ngx_proxy_wasm_store_destroy(ngx_proxy_wasm_store_t *store)
{
    ngx_queue_t                *q;
    ngx_proxy_wasm_instance_t  *ictx;

    dd("enter");

    while (!ngx_queue_empty(&store->busy)) {
        dd("remove busy");
        q = ngx_queue_head(&store->busy);
        ictx = ngx_queue_data(q, ngx_proxy_wasm_instance_t, q);

        ngx_queue_remove(&ictx->q);
        ngx_queue_insert_tail(&store->free, &ictx->q);
    }

    while (!ngx_queue_empty(&store->free)) {
        dd("remove free");
        q = ngx_queue_head(&store->free);
        ictx = ngx_queue_data(q, ngx_proxy_wasm_instance_t, q);

        ngx_queue_remove(&ictx->q);
        ngx_queue_insert_tail(&store->sweep, &ictx->q);
    }

    ngx_proxy_wasm_store_sweep(store);

    dd("exit");
}


static void
ngx_proxy_wasm_store_sweep(ngx_proxy_wasm_store_t *store)
{
    ngx_queue_t                *q;
    ngx_proxy_wasm_instance_t  *ictx;

    while (!ngx_queue_empty(&store->sweep)) {
        q = ngx_queue_head(&store->sweep);
        ictx = ngx_queue_data(q, ngx_proxy_wasm_instance_t, q);

        dd("sweep (ictx: %p)", ictx);

        ngx_queue_remove(&ictx->q);
        ngx_proxy_wasm_instance_destroy(ictx);
    }
}


#if 0
static void
ngx_proxy_wasm_store_schedule_sweep_handler(ngx_event_t *ev)
{
    ngx_proxy_wasm_store_t *store = ev->data;

    ngx_proxy_wasm_store_sweep(store);
}


static void
ngx_proxy_wasm_store_schedule_sweep(ngx_proxy_wasm_store_t *store)
{
    ngx_event_t  *ev;

    ev = ngx_calloc(sizeof(ngx_event_t), store->pool->log);
    if (ev == NULL) {
        return;
    }

    ev->handler = ngx_proxy_wasm_store_schedule_sweep_handler;
    ev->data = store;
    ev->log = store->pool->log;

    ngx_post_event(ev, &ngx_posted_events);
}
#endif
