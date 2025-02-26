# Metrics

This document elaborates on the types of metrics available in ngx_wasm_module,
how they are stored in memory, and how to estimate the amount of [slab_size]
memory necessary for your use-case.

## Table of Contents

- [Types of Metrics](#types-of-metrics)
- [Name Prefixing](#name-prefixing)
- [Histogram Binning Strategies](#histogram-binning-strategies)
    - [Logarithmic Binning](#logarithmic-binning)
    - [Custom Binning](#custom-binning)
- [Histogram Update and Expansion](#histogram-update-and-expansion)
- [Memory Consumption](#memory-consumption)
- [Shared Memory Allocation](#shared-memory-allocation)
- [Nginx Reconfiguration](#nginx-reconfiguration)

## Types of Metrics

In accordance with Proxy-Wasm specifications, a "metric" is either a counter, a
gauge, or a histogram.

- A counter is an unsigned 64-bit int that can only be incremented.
- A gauge is an unsigned 64-bit int that can take arbitrary values.
- A histogram represents range frequencies of a variable and can be defined as a
  set of pairs of ranges and counters.
  For example, the distribution of response time of HTTP requests can be
  represented as a histogram with ranges `[0, 1]`, `(1, 2]`, `(2, 4]`, and `(4,
  Inf]`. The 1st range counter would be the number of requests with response
  time less or equal to 1ms; the 2nd range counter represents requests with
  response time between 1ms and 2ms; the 3rd range counter are requests with
  response time between 2ms and 4ms; and the last range counter are requests
  with response time bigger than 4ms.

[Back to TOC](#table-of-contents)

## Name Prefixing

To avoid naming conflicts between Proxy-Wasm filters, the name of a metric is
always prefixed with colon-separated metadata: `pw:{filter_name}:`. This means
that a metric named `a_counter` inserted by `a_filter` will have its name stored
as: `pw:a_filter:a_counter`.

Thus, the maximum length of a metric name configured via
[max_metric_name_length] is enforced on the prefixed name and may need to be
increased in some cases.

[Back to TOC](#table-of-contents)

## Histogram Binning Strategies

### Logarithmic Binning

By default, histograms use a logarithmic-binning strategy. This is the only
available binning strategy when using the Proxy-Wasm SDK at this time.

As an example of logarithmic-binning, take the histogram with ranges (i.e.
"bins") `[0, 1] (1, 2] (2, 4] (4, Inf]`: each bin's upper-bound is growing in
powers of 2: `2^0`, `2^1`, and `2^2`. In logarithmic-binning, a value `v` being
recorded is matched with the smallest power of two that is bigger than `v`. This
value is the *upper-bound* of the bin associated with `v`. If the histogram
contains or can contain such a bin, then its counter is incremented. If not, the
bin with the next smallest upper-bound bigger than `v` has its counter
incremented instead.

In ngx_wasm_module, logarithmic-binning histograms are created with one
initialized bin with upper-bound `2^32`. The counter for this bin is incremented
if it is the only bin whose upper-bound is bigger than the recorded value.

When a value `v` is recorded and its bin does not yet exist, a new bin with the
upper-bound associated with `v` is initialized and its counter is incremented.

A logarithmic-binning histogram can contain up to 18 initialized bins.

[Back to TOC](#table-of-contents)

### Custom Binning

Through the Lua FFI library provided with this module, histograms can also be
created with a fixed set of bins with user-defined upper-bounds. These
histograms store values exactly like the logarithmic-binning ones, except the
number of bins and their upper-bounds are user-defined and pre-initialized.

A custom-binning histogram can contain up to 18 bins (17 user-defined bins + one
`2^32` upper-bound bin). Custom-binning histograms cannot be expanded with new
bins after definition.

[Back to TOC](#table-of-contents)

## Memory Consumption

The space occupied by a metric in memory contains:

1. Its name.
2. Its value.
3. And the underlying structure representing the metric in the shared key-value
   store memory (see [slab_size]).

While the key-value structure has a fixed size of **96 bytes**, the sizes of
name and value vary.

In memory, the value of a counter or gauge occupies 8 bytes + 16 bytes per
worker process. The value size grows according to the number of workers because
metric values are segmented across them: Each worker has its own segment of the
value to write updates to. When a metric is retrieved, the segments are
consolidated and returned as a single metric value. This storage strategy allows
metric updates to be performed without the aid of shared memory read/write locks
at the cost of 16 bytes per worker.

Histogram values have a baseline size of 8 bytes + 24 bytes per worker process.
However, histograms also need extra space per worker for bins storage.
Bins storage costs 4 bytes + 8 bytes per bin. Thus, a 5-bin histogram takes: 8
bytes + (24 + 4 + 5*8), so 68 bytes per worker.

As such, in a 4-workers setup, a counter or gauge whose name is 64 chars long
occupies 168 bytes, and a 5-bin histogram with the same name length occupies 408
bytes. A 18-bin histogram with the same length name occupies 856 bytes.

[Back to TOC](#table-of-contents)

## Shared Memory Allocation

Nginx employs a shared memory allocation model that enforces allocation size to
be a power of 2 greater than 8; nonconforming values are rounded up, see [Nginx
shared memory].

For instance, this means that an allocation of 168 bytes ends up occupying 256
bytes of shared memory. This should be taken into consideration when estimating
the total space required for a group of metrics.

[Back to TOC](#table-of-contents)

## Nginx Reconfiguration

If Nginx is reconfigured with a different number of workers or a different
[slab_size] value, existing metrics need to be reallocated into a new
shared memory zone at reconfiguration time. This is due to the metric values
being segmented across workers.

As such, it is important to make sure that the new [slab_size] value is large
enough to accommodate existing metrics, and that the value of
[max_metric_name_length] is not less than any existing metric name.

[Back to TOC](#table-of-contents)

[Nginx shared memory]: https://nginx.org/en/docs/dev/development_guide.html#shared_memory
[slab_size]: DIRECTIVES.md#slab_size
[max_metric_name_length]: DIRECTIVES.md#max_metric_name_length
