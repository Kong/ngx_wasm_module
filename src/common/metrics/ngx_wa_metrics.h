#ifndef _NGX_WA_METRICS_H_INCLUDED_
#define _NGX_WA_METRICS_H_INCLUDED_


#include <ngx_wasm_shm_kv.h>


#define ngx_wa_metrics_counter(m) m->slots[0].counter
#define ngx_wa_metrics_gauge(m) m->slots[0].gauge.value


#define NGX_WA_METRICS_BINS_INIT             5
#define NGX_WA_METRICS_BINS_MAX              18
#define NGX_WA_METRICS_BINS_INCREMENT        4
#define NGX_WA_METRICS_MAX_HISTOGRAM_SIZE    sizeof(ngx_wa_metrics_histogram_t)\
                                             + sizeof(ngx_wa_metrics_bin_t)    \
                                             * NGX_WA_METRICS_BINS_MAX
#define NGX_WA_METRICS_ONE_SLOT_METRIC_SIZE  sizeof(ngx_wa_metric_t)           \
                                             + sizeof(ngx_wa_metric_val_t)


typedef struct ngx_wa_metrics_s  ngx_wa_metrics_t;

typedef enum {
    NGX_WA_METRIC_COUNTER,
    NGX_WA_METRIC_GAUGE,
    NGX_WA_METRIC_HISTOGRAM,
} ngx_wa_metric_type_e;


typedef struct {
    ngx_uint_t                   value;
    ngx_msec_t                   last_update;
} ngx_wa_metrics_gauge_t;


typedef struct {
    uint32_t                     upper_bound;
    uint32_t                     count;
} ngx_wa_metrics_bin_t;


typedef struct {
    uint8_t                      n_bins;
    ngx_wa_metrics_bin_t         bins[];
} ngx_wa_metrics_histogram_t;


typedef union {
    ngx_uint_t                   counter;
    ngx_wa_metrics_gauge_t       gauge;
    ngx_wa_metrics_histogram_t  *histogram;
} ngx_wa_metric_val_t;


typedef struct {
    ngx_wa_metric_type_e         type;
    ngx_wa_metric_val_t          slots[];
} ngx_wa_metric_t;


typedef struct {
    size_t                       slab_size;
    size_t                       max_metric_name_length;
    unsigned                     initialized:1;
} ngx_wa_metrics_conf_t;


struct ngx_wa_metrics_s {
    ngx_uint_t                   workers;
    ngx_wasm_shm_t              *shm;
    ngx_wa_metrics_t            *old_metrics;
    ngx_wa_metrics_conf_t        config;
    ngx_wasm_shm_mapping_t      *mapping;
};


ngx_str_t *ngx_wa_metric_type_name(ngx_wa_metric_type_e type);

ngx_wa_metrics_t *ngx_wa_metrics_alloc(ngx_cycle_t *cycle);
char *ngx_wa_metrics_init_conf(ngx_wa_metrics_t *metrics, ngx_conf_t *cf);
ngx_int_t ngx_wa_metrics_init(ngx_cycle_t *cycle);

ngx_int_t ngx_wa_metrics_define(ngx_wa_metrics_t *metrics, ngx_str_t *name,
    ngx_wa_metric_type_e type, uint32_t *out);
ngx_int_t ngx_wa_metrics_increment(ngx_wa_metrics_t *metrics,
    uint32_t metric_id, ngx_int_t val);
ngx_int_t ngx_wa_metrics_record(ngx_wa_metrics_t *metrics, uint32_t metric_id,
    ngx_int_t val);
ngx_int_t ngx_wa_metrics_get(ngx_wa_metrics_t *metrics, uint32_t metric_id,
    ngx_wa_metric_t *o);

ngx_int_t ngx_wa_metrics_histogram_add_locked(ngx_wa_metrics_t *metrics,
    ngx_wa_metric_t *m);
ngx_int_t ngx_wa_metrics_histogram_record(ngx_wa_metrics_t *metrics,
    ngx_wa_metric_t *m, ngx_uint_t slot, uint32_t mid, ngx_uint_t n);
void ngx_wa_metrics_histogram_get(ngx_wa_metrics_t *metrics, ngx_wa_metric_t *m,
    ngx_uint_t slots, ngx_wa_metrics_histogram_t *out);


static ngx_inline ngx_wa_metrics_histogram_t *
ngx_wa_metrics_histogram_set_buffer(ngx_wa_metric_t *m, u_char *b, size_t s)
{
    m->slots[0].histogram = (ngx_wa_metrics_histogram_t *) b;
    m->slots[0].histogram->n_bins = (s - sizeof(ngx_wa_metrics_histogram_t))
                                    / sizeof(ngx_wa_metrics_bin_t);
    m->slots[0].histogram->bins[0].upper_bound = NGX_MAX_UINT32_VALUE;

    return m->slots[0].histogram;
}


#endif /* _NGX_WA_METRICS_H_INCLUDED_ */
