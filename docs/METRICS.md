# Metrics

## Introduction

In the context of ngx_wasm_module, in accordance with Proxy-Wasm, a metric is
either a counter, a gauge or a histogram.

A counter is an unsigned 64-bit int that can only be incremented.
A gauge is an unsigned 64-bit int that can take arbitrary values.

## Histograms

A histogram represents ranges frequencies of a variable and can be defined as a
set of pairs of range and counter. For example, the distribution of response
time of HTTP requests, can be represented as a histogram with ranges `[0, 1]`,
`(1, 2]`, `(2, 4]` and `(4, Inf]`. The 1st range's counter, would be the number
of requests with response time less or equal to 1ms; the 2nd range's counter,
requests with response time between 1ms and 2ms; the 3rd range's counter,
requests with response time between 2ms and 4ms; and the last range's counter,
requests with response time bigger than 4ms.

### Binning

The above example demonstrates a histogram with ranges, or bins, whose upper
bound grows in powers of 2, i.e. 2^0, 2^1 and 2^2. This is usually called
logarithmic binning and is indeed how histograms bins are represented in the
ngx_wasm_module. This binning strategy implicates that when a value `v` is
recorded, it is matched with the smallest power of two that's bigger than `v`;
this value is the upper bound of the bin associated with `v`; if the histogram
contain, or can contain, such bin, its counter is incremented; if not, the bin
with the next smallest upper bound bigger than `v` has its counter incremented.

### Update and expansion

Histograms are created with 5 bins, 1 initialized and 4 uninitialized. If a
value `v` is recorded and its bin isn't part of the initialized bins, one of the
uninitialized bins is initialized with the upper bound associated with `v` and
its counter is incremented. If the histogram is out of uninitialized bins, it
can be expanded, up to 18 bins, to accommodate the additional bin for `v`. The
bin initialized upon histogram creation has upper bound 2^32 and its counter is
incremented if it's the only bin whose upper bound is bigger than the recorded
value.

## Memory consumption

The space in memory occupied by a metric contains its name, value and the
underlying structure representing them in the key-value store. While the
key-value structure has a fixed size of 96 bytes, the sizes of name and value
vary.

The size in memory of the value of a counter or gauge is 8 bytes plus 16 bytes
per worker process. The value size grows according to the number of workers
because metric value is segmented across them. Each worker has its own segment
of the value to write updates to. When a metric is retrieved, the segments are
consolidated and returned as a single metric. This storage strategy allows
metric updates to be performed without the aid of locks at the cost of 16 bytes
per worker.

Histograms' values also have a baseline size of 8 bytes plus 16 bytes per
worker. However, histograms need extra space per worker for bins storage. Bins
storage costs 4 bytes plus 8 bytes per bin. So a 5-bin histogram takes 8 bytes
plus (16 + 4 + 5*8), 60 bytes per worker.

As such, in a 4-workers setup, a counter or gauge whose name is 64 chars long
takes 168 bytes, a 5-bin histogram with the same name takes 408 bytes and a
18-bin histogram with the same name takes 824 bytes.

### Shared memory allocation

Nginx employs an allocation model for shared memory that enforces allocation
size to be a power of 2 and greater than 8; nonconforming values are rounded up,
see [Nginx shared memory].

This means that an allocation of 168 bytes, for instance, ends up taking 256
bytes from the shared memory. This should be taken into account when estimating
the space required for a group of metrics.

### Prefixing

The name of a metric is always prefixed with `pw.{filter_name}.` to avoid naming
conflicts between Proxy-Wasm filters. This means that a metric named `a_counter`
by the filter `a_filter` ends up named as `pw.a_filter.a_counter`.
The maximum length of a metric name, configured via `max_metric_name_length`,
is enforced on the prefixed name and might need to be increased in some cases.

## Nginx Reconfiguration

If Nginx is reconfigured with a different number of workers or a different size
for the metrics shared memory zone, existing metrics need to be reallocated into
a brand new shared memory zone. This is due to the metric values being segmented
across workers.

As such, it's important to ensure a new size of the metrics' shared memory zone 
is enough to accommodate existing metrics and that the value of
`max_metric_name_len` isn't less than any existing metric name.

[Nginx shared memory]: https://nginx.org/en/docs/dev/development_guide.html#shared_memory
