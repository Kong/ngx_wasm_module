#ifndef _NGX_WA_HISTOGRAM_H_INCLUDED_
#define _NGX_WA_HISTOGRAM_H_INCLUDED_


#include <ngx_wa_metrics.h>


ngx_int_t ngx_wa_metrics_histogram_add_locked(ngx_wa_metrics_t *metrics,
    ngx_wa_metric_t *m);
ngx_int_t ngx_wa_metrics_histogram_record(ngx_wa_metrics_t *metrics,
    ngx_wa_metric_t *m, ngx_uint_t slot, uint32_t mid, ngx_uint_t n);
void ngx_wa_metrics_histogram_get(ngx_wa_metrics_t *metrics, ngx_wa_metric_t *m,
    ngx_uint_t slots, ngx_wa_metrics_histogram_t *out);


#endif /* _NGX_WA_HISTOGRAM_H_INCLUDED_ */
