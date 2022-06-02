# Embedding proxy-wasm filters in Kong Gateway

* Status: proposed
* Deciders: WasmX + Gateway teams
* Date: 2022-06-02

Technical Story: [FT-2846](https://konghq.atlassian.net/browse/FT-2846?jql=text%20~%20%22WasmX%22)

## Table of Contents

- [Problem Statement](#problem-statement)
- [Technical Context](#technical-context)
- [Decision Drivers](#decision-drivers)
- [Considered Options](#considered-options)
    - [1. Side-stepped runloop](#1-side-stepped-runloop)
    - [2. LuaJIT FFI bindings](#2-luajit-ffi-bindings)
    - [3. Gateway core integration](#3-gateway-core-integration)
    - [4. Gateway plugin integration](#4-gateway-plugin-integration)
- [Decision Outcomes](#decision-outcomes)

## Problem Statement

Execute a chain of proxy-wasm filters as part of the Gateway's request/response
life-cycle. How to integrate this new runloop alongside existing Lua plugins?

> How exactly is the proxy-wasm filter chain configured by the Control Plane
  (e.g. API endpoints and operations) remains outside of the scope of this
  proposal, although the proposed solutions must take this requirement in
  consideration.

[Back to TOC](#table-of-contents)

## Technical Context

Presently within ngx_wasm_module, the proxy-wasm runloop is entered and executed
via ngx_http_wasm_module, which populates Nginx extension points with said
runloop invocations (e.g. in "rewrite", "access", "body_filter" phases, etc...).

However, the Gateway is entirely built within *another* Nginx C module:
ngx_http_lua_module. Two Nginx modules implementing the same extension points
(e.g. "rewrite" for "on request headers") will see their respective handlers
executed subsequently, not intermittently.

Therefore, we need to combine our integration options such that the proxy-wasm
runloop is well encapsulated and can be invoked as a stateful "black box" both
from within the Gateway's Lua-land and from ngx_http_wasm_module.

[Back to TOC](#table-of-contents)

## Decision Drivers

* Minimizes engineering effort for integration within the Gateway.
* Maximizes configuration flexibility for Gateway users.
* First WebAssembly support in the Gateway, aim at increasing adoption.
* WasmX's first introduction into the Gateway runtime, aim at increasing
  ownership.
* Compromising between ngx_wasm_module design and the Gateway's wrapping
  "glue-code" (i.e. the Lua-land layer).

[Back to TOC](#table-of-contents)

## Considered Options

1. Side-stepped proxy-wasm runloop (i.e. Nginx subrequest).
2. LuaJIT FFI bindings.
3. Gateway core: proxy-wasm runloop module.
4. Gateway plugin: proxy-wasm runloop plugin.

[Back to TOC](#table-of-contents)

### 1. Side-stepped runloop

In this scenario, a "side runloop" is implemented alongside the Gateway's
runloop. The Gateway is implemented as an Nginx location handler so naturally,
this option would replicate this solution into another location handler:

```nginx
location / {
    access_by_lua_block {
        ngx.req.read_body()

        local res = ngx.location.capture("/wasm/req", {
            always_forward_body = true,
        })
    }

    proxy_pass [...];

    header_filter_by_lua_block {
        local res = ngx.location.capture("/wasm/resp", {
            always_forward_body = true,
        })

        ngx.status = res.status
        ngx.print(res.body)
    }
}

location /wasm/req {
    internal;
    proxy_wasm_req my_filter;
}

location /wasm/resp {
    internal;
    proxy_wasm_resp my_filter;
}
```

Pros:

* Fast integration within the Gateway, minimal initial effort from Gateway &
  ngx_wasm_module.
* Allows for "alpha" embedding of ngx_wasm_module & proxy-wasm SDK for initial
  testing and proof-of-concept.

Cons:

* Because implemented as a subrequest, the proxy-wasm runloop must only invoke
  request handlers during the request-subrequest, and response handlers only
  during the response-subrequest. This means some effort from ngx_wasm_module's
  side, although not too significant.
* Also because implemented as a subrequest, any request manipulation from within
  the filter needs to be reflected in the main request. Special care must be
  given to the subrequest dispatch and reading Lua code to "copy" changes made
  to the main request during the proxy-wasm processing (same goes for changes
  made during the response cycle).
* Because integration depends on nginx.conf directives, this approach would
  require generating the relevant nginx.conf section to inject the `proxy_wasm`
  directives. This is possible within the Gateway, but means that configuration
  or ordering changes of the proxy-wasm filter require at least an Nginx reload.

[Back to TOC](#table-of-contents)

### 2. LuaJIT FFI bindings

In this scenario, ngx_wasm_module provides a stable C ABI that the Gateway can
wrap with a LuaJIT FFI layer:

```lua
-- lua-resty-proxy-wasm

ffi.cdef [[
int ngx_wasm_ops_resume(ngx_wasm_op_ctx *ctx, int step);
]]

function _M.resume(ctx, step)
    return ffi.C.ngx_wasm_ops_resume(ctx, step)
end
```

This FFI interface is then invoked from anywhere within Lua-land, as such:

```lua
local proxy_wasm = require "resty.proxy_wasm"

local res = proxy_wasm.resume(ctx, request_headers_step)
```

Note that the proxy-wasm filter chain can thus be resumed from anywhere, but
needs to maintain an internal state. As such, the client is responsible for
communicating to the chain which step it wishes to resume (e.g. "request
headers" or "response headers"). This is similar to ngx_http_wasm_module's
invocation of the chain today.

> Note as well that `ngx_wasm_ops` is sitting at a higher-level of abstraction
  than `ngx_proxy_wasm`. Ultimately, `ngx_wasm_ops` is a "chain of WebAssembly
  operations to be executed". These operations can be "run a proxy-wasm filter",
  or "do something else with WebAssembly" if we so desire in the future.

Pros:

* Solidifying the proxy-wasm ABI will improve its encapsulation and increase
  code quality of both ngx_http_wasm_module and that of the LuaJIT wrapper.
* Most flexible approach for Gateway integration. By decoupling the filter
  chain's state from the caller's state, we ensure that the filter chain can be
  invoked anywhere it needs to be, and keep the door open to more complex
  invocations in the future (e.g. from an Nginx error page location handler if
  need be).
* This approach lays out the foundations for dynamically configuring a chain of
  filters from the Gateway's Control Plane: since each filter is a "wasm
  operation" (i.e. `ngx_wasm_op`), a C ABI can be developed to manipulate the
  chain of wasm operations at runtime, for example:
  `ngx_wasm_ops_add_filter(chain_ctx, filter_bytes, filter_config)`.

Cons:

* More work required compared to the "side-stepped runloop", for both
  ngx_wasm_module and the Gateway: ngx_wasm_module will need some code
  reorganization/solidification, and the Gateway needs a LuaJIT wrapping layer.
  Ultimately, this solution would have to be gradually implemented and iterated
  upon.

[Back to TOC](#table-of-contents)

### 3. Gateway core integration

Assuming the availability of a LuaJIT wrapper library (lua-resty-proxy-wasm,
depending on the C ABI proposal), the question remains of where exactly should
the proxy-wasm runloop be invoked from.

The first solution would be to invoke a configured filter chain from within the
Gateway:

```lua
-- kong/runloop/plugins_iterator.lua

local proxy_wasm = require "resty.proxy_wasm"

local res = proxy_wasm.resume(ctx, request_headers_step)
```

Similarly to the Lua plugins runloop, this new runloop would be invoked during
the processing of the request/response life-cycle, either:

- _before_ the execution of Lua plugins,
- or _after_ the execution of Lua plugins.

A second, more elaborate approach, would be to consider the creation of a new
"type of plugins": instead of Lua plugins only, the runloop would also support
"proxy-wasm filters". This new type of plugin can be implemented as a sub-class
of the Gateway's "Plugin" entity, which would be distinguished from a Lua Plugin
entity.

Pros:

* Takes another step forward (after plugins dynamic ordering) in improving the
  Gateway runloop by supporting a new type of plugins: proxy-wasm filters.
* Maintains a strong "Kong Gateway centric" viewpoint, in coherence with the
  Gateway's established data-model.

Cons:

* This approach is heavier on the Gateway side, since it would require
  additional development for the specification of a new core data-model entity
  along with the updates of a core component (the plugins runloop).

[Back to TOC](#table-of-contents)

### 4. Gateway plugin integration

Another approach would be implementing a proxy-wasm runloop as a Lua plugin,
effectively embedding an entire proxy-wasm filter chain inside of a single
plugin and invoking the proper handler:

```lua
-- kong/plugins/proxy-wasm/handler.lua

local proxy_wasm = require "resty.proxy_wasm"

local ProxyWasmHandler = {}

function ProxyWasmHandler:access(conf)
    local res = proxy_wasm.resume(ctx, request_headers_step)
end

return ProxyWasmHandler
```

Pros:

* Less invasive integration compared to a core runloop update.
* A user configuring a proxy-wasm filter chain can leverage the modularity of
  existing plugins: if plugins can be re-ordered, then the user can chose to
  execute a chain of filter at anytime in-between the execution of other Lua
  plugins. Additionally, if the same plugin can be executed several times during
  a request/response life-cycle (with different configurations), then several
  proxy-wasm runloop plugins could be configured at a time.
  For example:
    1. execute Lua plugin A
    2. then proxy-wasm filters X & Y
    3. then Lua plugin B
    4. then proxy-wasm filter Z

Cons:

* Unless elegantly abstracted for the end-user, this approach would feel less
  "native" compared to a core integration. In this context, the Gateway would
  only support a *Lua plugin that runs WebAssembly*. The maturity level of Wasm
  support would be perceived differently from a core integration with a core
  "WebAssembly Plugin" entity.
* This approach limits the flexibility of proxy-wasm invocations: since they can
  only be invoked from within a Lua plugin, the Gateway therefore would only
  support proxy-wasm where Lua plugins are already supported. For example,
  invoking WebAssembly during Nginx's error handling would require that Lua
  plugins be invoked on Nginx's error handling code path.

[Back to TOC](#table-of-contents)

## Decision Outcomes

From the above proposal, it appears that a side-stepped runloop (1.) would only
allow for a temporary integration while also requiring temporary changes to
ngx_wasm_module (`proxy_wasm_req`/`proxy_wasm_resp` directives) to be fully
functional.

The C ABI wrapped by LuaJIT's FFI (2.) should become the long term approach for
this integration, as it is both the most flexible and future-proof design. The
WasmX team should start focusing its efforts towards the design and
specification of a robust C ABI and propose another ADR for it, evaluating
necessary changes inside ngx_wasm_module.

Whether the resulting proxy-wasm runloop (Lua-wrapped) is invoked from within
the Gateway's core (3.) or from a Lua plugin (4.) remains to be decided, perhaps
with a product-driven mindset.

[Back to TOC](#table-of-contents)
