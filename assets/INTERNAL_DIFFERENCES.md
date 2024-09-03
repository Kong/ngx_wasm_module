# Host SDK implementation differences

This document lists internal discrepancies between ngx_wasm_module and other
Proxy-Wasm host SDK implementations, e.g. Envoy, and it's meant to be used by
ngx_wasm_module's developers rather than its users.

## Table of Contents

- [Metrics Prefixing](#metrics-prefixing)

## Metrics Prefixing

In ngx_wasm_module, Proxy-Wasm metric names are prefixed with `pw.filter_name.`,
where `filter_name` represents the name assigned to the filter in which the
metric is defined. A metric `m1` defined by a filter `f1` is represented
internally as `pw.f1.m1`.

Envoy, however, prefixes Proxy-Wasm metric names `wasmcustom.`. As such, a
metric `m1` defined by a filter `f1` is represented internally as
`wasmcustom.m1`.

Proxy-Wasm filter developers don't need to be aware of such divergence as they
never use the metric name to retrieve or update a metric's value.

In fact, this difference shouldn't affect users at all, as long as host
implementations expose metric names without their internal prefixing.

Although Envoy will expose its internal prefixing in responses to the
`/metrics` admin endpoint, it won't do it in responses to the
`/metrics/prometheus` admin endpoint.

ngx_wasm_module follows a similar pattern. Although its internal prefixing is
exposed in metric names retrieved by its FFI, this interface is intended to be
accessed by Kong Gateway developers only, not end-users. Kong Gateway in turn
strips the internal prefixing before serializing the metrics in responses to the
`/metrics` admin endpoint.
