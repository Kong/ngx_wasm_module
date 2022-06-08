# Proxy-wasm C ABI

* Status: proposed
* Deciders: WasmX
* Precedents: [ADR 001]
* Date: 2022-06-08

Technical Story: [FT-2846](https://konghq.atlassian.net/browse/FT-2846?jql=text%20~%20%22WasmX%22)

## Table of Contents

- [Problem Statement](#problem-statement)
- [Technical Context](#technical-context)
- [Decision Drivers](#decision-drivers)
- [Proposal](#proposal)
    - [Known Limitations](#known-limitations)
- [Decision Outcomes](#decision-outcomes)

## Problem Statement

Define an initial C ABI for Foreign Function Interface (FFI) integration of
proxy-wasm operations. Expose ngx_wasm_module's proxy-wasm implementation with
an OpenResty integration in mind, and identify ngx_wasm_module necessary changes
or contention points.

> How exactly is this ABI embedded inside of the Gateway and how it is
  configured by users remain outside of the scope of this proposal, and will be
  the focus of a subsequent ADR.

[Back to TOC](#table-of-contents)

## Technical Context

The proposed ABI will be consumed by a LuaJIT wrapper, itself exposing a
lua-resty library of Wasm operations. This lua-resty library will in turn be
consumed by the Gateway, either as a plugin or a core component as proposed in
[ADR 001], or both.

[Back to TOC](#table-of-contents)

## Decision Drivers

- Aim at a "first-pass" working ABI that can be iterated upon.
- Capitalize on the abstractions already introduced by ngx_wasm_module.
- Do not compromise future dynamic filter loading.

[Back to TOC](#table-of-contents)

## Proposal

The decision to expose the proxy-wasm via the LuaJIT FFI having been made in
[ADR 001], this proposal focused on the design of the ABI.

The proposed initial ABI is concise and minimalistic, with the aim of providing
a "first-pass" that can be iterated upon.

```c
/* wasm */
typedef struct ngx_wavm_t             ngx_wasm_vm_t;
typedef struct ngx_wasm_ops_engine_t  ngx_wasm_ops_t;

ngx_wasm_vm_t *ngx_wasm_ffi_main_vm();

/* proxy-wasm */
typedef struct {
    ngx_str_t        *name;
    ngx_str_t        *config;
} ngx_wasm_filter_t;

ngx_wasm_ops_t *ngx_http_wasm_ffi_pwm_new(ngx_wasm_vm_t *vm,
    ngx_wasm_filter_t **filters, size_t n_filters);
int ngx_http_wasm_ffi_pwm_resume(ngx_http_request_t *r, ngx_wasm_ops_t *ops,
    ngx_uint_t phase);
int ngx_http_wasm_ffi_pwm_cancel(ngx_http_request_t *r, ngx_wasm_ops_t *ops);
void ngx_http_wasm_ffi_pwm_free(ngx_wasm_ops_t *ops);
```

Comments:

- A generic "get VM" method, which returns the one only "main VM" currently
  started by ngx_wasm_module. More methods could be added to expose the VM state
  such as "load module bytes", "get module by name", etc... Later iterations of
  this interface may or may not continue exposing the VM.
- All Wasm invocations revolve around `ngx_wasm_ops` which is an abstract
  pipeline of Wasm operations. Its typedef signals this type may evolve and
  become abstracted in the future.
- A "chain of filters" is created (i.e. a pipeline of "wasm operations") via
  `pwm_new`, which finds the filters in the Wasm VM's loaded modules,
  initializes them, and ties them together in a prepared `ngx_wasm_ops`
  pipeline.
- The chain of filters or `ngx_wasm_ops` is resumed from within a request, once
  for each phase via `pwm_resume`. The underlying `ngx_wasm_ops_engine` and
      `ngx_proxy_wasm` routines invoke proxy-wasm callbacks appropriately. If
      one of the filter yields, it is expected of the Nginx C event handler to
      resume the filter chain without `pwm_resume` having to be invoked again.
- LuaJIT can tie `pwm_free` to the GC cycle of the underlying "cdata" object
  representing `ngx_wasm_ops`, which can be a good first pass for this stage.

[Back to TOC](#table-of-contents)

### Known Limitations

- Currently, proxy-wasm phases are tied to Nginx phases by `ngx_wasm_ops`. This
  means the Gateway does not have direct control over proxy-wasm phases, which
  may or may not become an obstacle in the future, depending on Gateway needs.
  Directly exposing proxy-wasm phases in the ABI have to be an iteration and
  updates to the `ngx_wasm_ops` and `ngx_proxy_wasm` systems.
- The initial ABI does not yet offer a way to configure any Wasm property of the
  VM, such as compilers or instance isolation.

[Back to TOC](#table-of-contents)

## Decision Outcomes

Let us reiterate that by implementing this ABI, a lua-resty library (i.e. LuaJIT
wrapper) will be provided and allow for execution of proxy-wasm filter chains
from Lua, which should provide a high level of invocation flexibility from
within OpenResty and the Gateway.

[ADR 003] will focus on the integration of this lua-resty wrapper inside of the
Gateway.

[Back to TOC](#table-of-contents)

[ADR 001]: assets/adr/001-proxy-wasm-gateway-embedding.md
[ADR 001]: assets/adr/003-proxy-wasm-gateway-plugin.md
