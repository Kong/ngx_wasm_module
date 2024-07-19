#ifndef _NGX_WA_METRICS_H_INCLUDED_
#define _NGX_WA_METRICS_H_INCLUDED_


#include <ngx_wasm_shm_kv.h>


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
    ngx_uint_t *out);


#endif /* _NGX_WA_METRICS_H_INCLUDED_ */
