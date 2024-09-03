# Proxy-Wasm Host Differences

Listed here are noteworthy internal discrepancies (implementation differences)
between ngx_wasm_module and other Proxy-Wasm host implementations.

## Table of Contents

- [Metrics Prefixing](#metrics-prefixing)

## Metrics Prefixing

- Envoy internally prefixes metric names with `wasmcustom.*` and only exposes
  this prefix in the output of the `/metrics` endpoint.
- For internal implementation reasons, ngx_wasm_module prefixes metric names
  with `pw.[filter_name].*`, where `filter_name` is the name of the filter
  that defined the metric. Only the ngx_wasm_module FFI library may expose the
  existence of this prefix.

Proxy-Wasm SDK users need not worry as metric names are never exposed through
the SDK in the first place.

[Back to TOC](#table-of-contents)
