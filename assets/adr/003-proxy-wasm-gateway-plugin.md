# Proxy-wasm Gateway Plugin

* Status: proposed
* Deciders: WasmX + Gateway
* Precedents: [ADR 001] & [ADR 002]
* Date: 2022-06-09

Technical Story: [FT-2846](https://konghq.atlassian.net/browse/FT-2846?jql=text%20~%20%22WasmX%22)

## Table of Contents

- [Problem Statement](#problem-statement)
- [Decision Drivers](#decision-drivers)
- [Proposal](#proposal)
    - [User Flow](#user-flow)
    - [Implementation Steps](#implementation-steps)
    - [Known Limitations](#known-limitations)
- [Decision Outcomes](#decision-outcomes)

## Problem Statement

Enable proxy-wasm filters in Kong Gateway by embedding the proxy-wasm ABI (ADR
002) in a Kong Gateway Plugin (ADR 001). Define the characteristics of this
plugin, how it integrates inside of the Gateway and how it can evolve.

[Back to TOC](#table-of-contents)

## Decision Drivers

- Aim at a "first-pass" working integration that can be iterated upon.
- Capitalize on the abstractions already provided by the Gateway and its
  plugins.

[Back to TOC](#table-of-contents)

## Proposal

The decision to initially integrate the proxy-wasm ABI ([ADR 002]) as a Gateway
Lua plugin having been made (proposed in [ADR 001] and accepted in follow-up
team gatherings), this proposal focuses on documenting:

- End-user journey for configuring and using this plugin.
- Implementation steps to reach this stage of integration.
- Known limitations at this stage of integration.

[Back to TOC](#table-of-contents)

### User Flow

The proposed user flow targets a fast working version that can allow for early
feedback aggregation and product refinement.

"As a Gateway user and proxy-wasm filter author, I should follow the subsequent
workflow when I wish to configure proxy-wasm filters on a Service:"

1. Write a proxy-wasm filter with the language SDK of my choice among the
   available ones.
2. Compile said filter to Wasm bytecode (`.wasm`) using the language's Wasm
   compiler.
3. Edit my Gateway configuration to define a proxy-wasm filter with a name and a
   path to the produced `.wasm` bytecode, on disk.
4. Configure a Plugin tied to the Service; the plugin's name is `proxy-wasm` and
   its configuration schema is an array of: `[(filter_name: str, filter_config:
   str), ...]`. I expect filters to be executed in the order in which they are
   defined.

[Back to TOC](#table-of-contents)

### Implementation Steps

1. ngx_wasm_module: Implement the lua-resty-wasm FFI wrapper library.
2. Gateway: compile with ngx_wasm_module.
3. Gateway: add proxy-wasm configuration properties and auto-generate `wasm{}`
   section in the Nginx configuration template.
4. Gateway: implement a Lua plugin leveraging lua-resty-wasm to enter the filter
   chain during a request.
    - declaratively-configured proxy-wasm chains must be created from within
      `init_worker` when available (e.g. background ticks).
5. Test cases, behavior checks, edge-cases and bug squashing.

Since dynamic plugin ordering landed in the Gateway, users will be able to
inject the proxy-wasm filter chain in-between two other plugins of their choice.

[Back to TOC](#table-of-contents)

### Known Limitations

A list of known limitations at the stage described by this proposal:

- The `.wasm` bytecode of proxy-wasm filters must be stored on disk (since we
  point to it via the `wasm{module}` directive).
- Loading a new proxy-wasm filter (`.wasm` from disk) requires an Nginx reload.
- There is no visibility into the Wasm and proxy-wasm states from a
  configuration assessment or telemetry standpoint.
- Since filter chains are tied to the `proxy-wasm` Lua plugin, filter chains can
  only be entered where Lua plugin are entered.
- Since plugins cannot be configured more than once on a given Gateway entity,
  only one filter chain can be injected for a request at a time.
- Filters and plugins will be isolated, with no way to interact (e.g. "shared"
  `ngx.ctx`).
- ngx_wasm_module's implementation of proxy-wasm SDK is still in progress (e.g.
  no shared k/v stores, no shared queues, no HTTPS calls...).
- I/O is only possible in the form of HTTP dispatch calls (plain-text) resolved
  by Nginx's `resolver` directive (instead of the Gateway's Lua-land resolver).

[Back to TOC](#table-of-contents)

## Decision Outcomes

This first stage of integration will allow for internal usage & testing, and
will help us taking product-driven decisions as to the following steps in
tightening the WebAssembly Gateway integration.

[Back to TOC](#table-of-contents)

[ADR 001]: assets/adr/001-proxy-wasm-gateway-embedding.md
[ADR 002]: assets/adr/002-proxy-wasm-abi.md
