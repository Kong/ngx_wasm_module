#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>
#include <ngx_wa_metrics.h>


static uint32_t
bin_log2_upper_bound(ngx_uint_t n)
{
    uint32_t upper_bound = 2;

    if (n <= 1) {
        return 1;
    }

    if (n > NGX_MAX_UINT32_VALUE / 2) {
        return NGX_MAX_UINT32_VALUE;
    }

    for (n = n - 1; n >>= 1; upper_bound <<= 1) { /* void */ }

    return upper_bound;
}


static ngx_int_t
histogram_grow(ngx_wa_metrics_t *metrics, ngx_wa_metrics_histogram_t *h,
    ngx_wa_metrics_histogram_t **out)
{
    size_t                       old_size, size;
    ngx_int_t                    rc = NGX_OK;
    ngx_uint_t                   n;
    ngx_wa_metrics_histogram_t  *new_h = NULL;

    if (h->n_bins == NGX_WA_METRICS_BINS_MAX) {
        return NGX_ERROR;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_WASM, metrics->shm->log, 0,
                   "growing histogram");

    n = ngx_min(NGX_WA_METRICS_BINS_INCREMENT,
                NGX_WA_METRICS_BINS_MAX - h->n_bins);
    old_size = sizeof(ngx_wa_metrics_histogram_t)
               + sizeof(ngx_wa_metrics_bin_t) * h->n_bins;
    size = old_size + sizeof(ngx_wa_metrics_bin_t) * n;

    if (metrics->shm->eviction == NGX_WA_SHM_EVICTION_NONE) {
        ngx_wa_shm_lock(metrics->shm);
    }

    new_h = ngx_slab_calloc_locked(metrics->shm->shpool, size);
    if (new_h == NULL) {
        ngx_log_debug0(NGX_LOG_DEBUG_WASM, metrics->shm->log, 0,
                       "cannot expand histogram");
        rc = NGX_ERROR;
        goto error;
    }

    ngx_memcpy(new_h, h, old_size);
    ngx_slab_free_locked(metrics->shm->shpool, h);

    new_h->n_bins += n;
    *out = new_h;

error:

    if (metrics->shm->eviction == NGX_WA_SHM_EVICTION_NONE) {
        ngx_wa_shm_unlock(metrics->shm);
    }

    return rc;
}


static ngx_wa_metrics_bin_t *
histogram_bin(ngx_wa_metrics_t *metrics, ngx_wa_metrics_histogram_t *h,
    ngx_uint_t n, ngx_wa_metrics_histogram_t **out)
{
    size_t                 i, j = 0;
    uint32_t               ub = bin_log2_upper_bound(n);
    ngx_wa_metrics_bin_t  *b;

    for (i = 0; i < h->n_bins; i++) {
        b = &h->bins[i];
        j = (j == 0 && ub < b->upper_bound) ? i : j;

        if (b->upper_bound == ub) {
            return b;
        }

        if (b->upper_bound == NGX_MAX_UINT32_VALUE) {
            i++;
            break;
        }
    }

    if (i == h->n_bins) {
        if (out && histogram_grow(metrics, h, out) == NGX_OK) {
            h = *out;

        } else {
            ngx_wasm_log_error(NGX_LOG_INFO, metrics->shm->log, 0,
                               "cannot add a new histogram bin for value "
                               "\"%uD\", returning next closest bin", n);
            return &h->bins[j];
        }
    }

    /* shift bins to create space for the new one */
    ngx_memcpy(&h->bins[j + 1], &h->bins[j],
               sizeof(ngx_wa_metrics_bin_t) * (i - j));

    h->bins[j].upper_bound = ub;
    h->bins[j].count = 0;

    return &h->bins[j];
}


#if (NGX_DEBUG)
static void
histogram_log(ngx_wa_metrics_t *metrics, ngx_wa_metric_t *m, uint32_t mid)
{
    size_t                       i;
    u_char                      *p;
    ngx_wa_metrics_bin_t        *b;
    ngx_wa_metrics_histogram_t  *h;
    u_char                       h_buf[NGX_WA_METRICS_MAX_HISTOGRAM_SIZE];
    u_char                       s_buf[NGX_MAX_ERROR_STR];

    ngx_memzero(h_buf, sizeof(h_buf));

    p = s_buf;
    h = (ngx_wa_metrics_histogram_t *) h_buf;
    h->n_bins = NGX_WA_METRICS_BINS_MAX;
    h->bins[0].upper_bound = NGX_MAX_UINT32_VALUE;

    ngx_wa_metrics_histogram_get(metrics, m, metrics->workers, h);

    for (i = 0; i < h->n_bins; i++) {
        b = &h->bins[i];
        p = ngx_sprintf(p, " %uD: %uD;", b->upper_bound, b->count);

        if (b->upper_bound == NGX_MAX_UINT32_VALUE) {
            break;
        }
    }

    ngx_log_debug3(NGX_LOG_DEBUG_WASM, metrics->shm->log, 0,
                   "histogram \"%uD\": %*s",
                   mid, p - s_buf - 1, s_buf + 1);
}
#endif


ngx_int_t
ngx_wa_metrics_histogram_add_locked(ngx_wa_metrics_t *metrics,
    ngx_wa_metric_t *m)
{
    size_t                        i;
    static uint16_t               n_bins = NGX_WA_METRICS_BINS_INIT;
    ngx_wa_metrics_histogram_t  **h;

    for (i = 0; i < metrics->workers; i++) {
        h = &m->slots[i].histogram;
        *h = ngx_slab_calloc_locked(metrics->shm->shpool,
                                    sizeof(ngx_wa_metrics_histogram_t)
                                    + sizeof(ngx_wa_metrics_bin_t) * n_bins);
        if (*h == NULL) {
            goto error;
        }

        (*h)->n_bins = n_bins;
        (*h)->bins[0].upper_bound = NGX_MAX_UINT32_VALUE;
    }

    return NGX_OK;

error:

    ngx_wasm_log_error(NGX_LOG_ERR, metrics->shm->log, 0,
                       "cannot allocate histogram");

    for (/* void */ ; i > 0; i--) {
        ngx_slab_free_locked(metrics->shm->shpool, m->slots[i - 1].histogram);
    }

    return NGX_ERROR;
}


ngx_int_t
ngx_wa_metrics_histogram_record(ngx_wa_metrics_t *metrics, ngx_wa_metric_t *m,
    ngx_uint_t slot, uint32_t mid, ngx_uint_t n)
{
    ngx_wa_metrics_bin_t        *b;
    ngx_wa_metrics_histogram_t  *h;

    h = m->slots[slot].histogram;
    b = histogram_bin(metrics, h, n, &m->slots[slot].histogram);
    b->count += 1;

#if (NGX_DEBUG)
    histogram_log(metrics, m, mid);
#endif

    return NGX_OK;
}


void
ngx_wa_metrics_histogram_get(ngx_wa_metrics_t *metrics, ngx_wa_metric_t *m,
    ngx_uint_t slots, ngx_wa_metrics_histogram_t *out)
{
    size_t                       i, j = 0;
    ngx_wa_metrics_bin_t        *b, *out_b;
    ngx_wa_metrics_histogram_t  *h;

    for (i = 0; i < slots; i++) {
        h = m->slots[i].histogram;

        for (j = 0; j < h->n_bins; j++) {
            b = &h->bins[j];
            out_b = histogram_bin(metrics, out, b->upper_bound, NULL);
            out_b->count += b->count;

            if (b->upper_bound == NGX_MAX_UINT32_VALUE) {
                break;
            }
        }
    }
}
