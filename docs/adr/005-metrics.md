# Metrics

* Status: proposed
* Deciders: WasmX
* Date: 2024-05-03

## Table of Contents

- [Problem Statement](#problem-statement)
- [Technical Context](#technical-context)
- [Decision Drivers](#decision-drivers)
- [Proposal](#proposal)
    - [Histograms](#histograms)
        - [Binning Strategy](#binning-strategy)
        - [Allocation and Updates](#allocation-and-updates)
        - [Expansion](#expansion)

## Problem Statement

Support the definition, update, and retrieval of metrics from Proxy-Wasm
filters, ngx_wasm_module itself, and the Lua land. How exactly are metrics
stored and how to access them so as to ensure two Nginx workers never write to
the same memory space.

[Back to TOC](#table-of-contents)

## Technical Context

A metric can either be a counter, a gauge, or a histogram:

- A counter is an integer that can only be incremented.
- A gauge is an integer that can take arbitrary positive values.
- A histogram represents range frequencies of a variable and can be defined as a
  set of pairs of ranges and counters.
  For example, the distribution of response time of HTTP requests can be
  represented as a histogram with ranges `[0, 1]`, `(1, 2]`, `(2, 4]`, and `(4,
  Inf]`. The 1st range counter would be the number of requests with response
  time less or equal to 1ms; the 2nd range counter represents requests with
  response time between 1ms and 2ms; the 3rd range counter are requests with
  response time between 2ms and 4ms; and the last range counter are requests
  with response time bigger than 4ms.

A metric's value should reflect updates from all worker processes. As a counter
begins at `0` and is incremented by workers 0 and 1, it should be `2` when
another worker retrieves its value. A gauge, however, is whatever value was last
set by any of the workers. Histograms, like counters, account for values
recorded by all workers.

[Back to TOC](#table-of-contents)

## Decision Drivers

* Full Proxy-Wasm ABI compatibility.
* Build atop ngx_wasm_shm.
* Minimize memory usage.
* Minimize metrics access cost.
* Generalize metrics ABI beyond Proxy-Wasm facilities (WasmX-wide).

[Back to TOC](#table-of-contents)

## Proposal

The proposed scheme for metrics storage builds atop ngx_wasm_shm's key-value
store. Metric name is stored as a key in a red-black tree node along with the
metric value. Metric values are represented by `ngx_wa_metric_t`, see below. The
member `type` is the metric type while the flexible array member `slots` stores
actual metric data.

The length of `slots` equals the number of worker processes running when the
metric is defined. This ensures each worker has its own dedicated slot to write
metric updates.

For counters, each entry in the `slots` array is simply an unsigned integer that
its corresponding worker increments. When a counter is retrieved, all values in
the `slots` array are summed and that total sum is returned.

For gauges, each of the `slots` is a pair of unsigned integer and timestamp.
When a worker sets a gauge, the value is stored along with the current time in
its corresponding worker slot. When a gauge is retrieved, the values in the
`slots` are iterated and the most recent value is returned.

For histograms, each of the `slots` points to a `ngx_wa_metrics_histogram_t`
instance. Each worker updates the histogram pointed to by its slot. When a
histogram is retrieved, the `slots` array is iterated and each worker's
histogram is merged into a temporary histogram, which can then be serialized.

```c
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
```

This storage strategy ensures that two workers **never** write to the same
memory address when updating a metric as long as no memory allocation is
performed. This is indeed the case for counters and gauges and it is also the
case for most histogram updates.

This is an important feature of this design as it allows the more frequent
update operations to be performed without the aid of shared memory locks. The
cost of a lock-less metric update then becomes merely the cost of searching the
red-black tree of the underlying key-value store: `O(log n)`.

The capacity of updating a metric without having to acquire a lock is
particularly attractive when a set of worker processes is under heavy load. In
such conditions, lock contention is likely to impact proxy throughput as workers
are more likely to wait for a lock to be released before proceeding with the
metric update.

Metric definition and removal still require locks to be safely performed as two
workers might end up attempting to write to the same memory location. This is
also true for histogram updates which cause them to grow their number of `bins`.

The ABI proposed to accomplish the described system closely resembles the one
from Proxy-Wasm specification itself:

```c
ngx_int_t ngx_wa_metrics_define(ngx_wa_metrics_t *metrics, ngx_str_t *name,
    ngx_wa_metric_type_e type, uint32_t *out);
ngx_int_t ngx_wa_metrics_increment(ngx_wa_metrics_t *metrics,
    uint32_t metric_id, ngx_int_t val);
ngx_int_t ngx_wa_metrics_record(ngx_wa_metrics_t *metrics, uint32_t metric_id,
    ngx_int_t val);
ngx_int_t ngx_wa_metrics_get(ngx_wa_metrics_t *metrics, uint32_t metric_id,
    ngx_uint_t *out);
```

[Back to TOC](#table-of-contents)

### Histograms

This proposal includes a scheme composed of `ngx_wa_metrics_bin_t` (a pair of
upper-bound and counter) and `ngx_wa_metrics_histogram_t` (a list of
`ngx_wa_metrics_bin_t` ordered by upper-bound) to represent histogram data in
memory. A bin's counter is the number of recorded values less than or equal to
its upper-bound and bigger than the previous bin's upper-bound.

This storage layout can represent both histograms with user-defined bins and
histograms following an automatic binning strategy, like logarithmic binning.
This document will focus, however, on logarithmic binning only; user-defined
bins are left for a future iteration.

[Back to TOC](#table-of-contents)

#### Binning Strategy

The proposed binning strategy assumes the domain of the variables being measured
is the set of non-negative integers and divides this domain into bins whose
upper-bound grows in powers of 2, i.e., 1, 2, 4, 8, 16, etc. The mapping of a
value `v` to its bin is given by the function `pow(2, ceil(log2(v)))` which
calculates the bin's upper-bound. The value 10, for example, is mapped to the
bin whose upper-bound is `pow(2, ceil(log2(10)))`, or 16. The bin with
upper-bound `16` represents recorded values between `8` and `16`.

This logarithmic scaling provides good enough resolution for small values in
return for low resolution for large values while keeping the memory footprint
reasonably low: values up to 65,536 can be represented with only 16 bins. These
characteristics fit the typical use case of measuring HTTP response time in
milliseconds.

[Back to TOC](#table-of-contents)

#### Allocation and Updates

Histograms are created with enough space for 5 bins, one of which is initialized
with `NGX_MAX_UINT32_VALUE` as upper-bound, leaving 4 uninitialized bins.

If a value `v` is recorded into a histogram and its respective bin is part of
the histogram's bins, its counter is simply incremented. If not and there is at
least one uninitialized bin, then one bin is initialized with the `v`
upper-bound, and the bins are rearranged to ensure ascending order with respect
to upper-bound. The new bin's counter is then incremented.

[Back to TOC](#table-of-contents)

#### Expansion

If a value `v` is recorded but its bin is not part of the histogram's bins and
there are not any uninitialized bins left, the histogram needs to grow to
accommodate the new value's bin.

Expanding a histogram means allocating memory for a new histogram instance with
enough space for the additional bin, then copying memory from the old instance
to the new one, and finally releasing the old histogram from memory. The new
uninitialized bin is then initialized with the `v` upper-bound and its counter
is incremented.

Histograms, however, can only grow up to a maximum number of bins. When a value
`v` is recorded into a histogram but its bin is not part of the bins and the
histogram has reached the total bin limit, the bin with the smallest upper-bound
bigger than `v` is incremented instead.

[Back to TOC](#table-of-contents)
