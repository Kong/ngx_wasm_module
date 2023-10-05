# Table of Contents

- [0.1.1]
- [0.1.0]

## Documentation

All releases are documented in a "git book" hosted in the project's repository:
the [user
documentation](https://github.com/Kong/ngx_wasm_module/tree/main/docs). **When
consulting any file under this location, make sure to checkout the proper
revision tag for your release.**

To install the module, consult
[INSTALL.md](https://github.com/Kong/ngx_wasm_module/tree/main/docs/INSTALL.md).

For Nginx users, the module can be used through `nginx.conf` directives,
documented in
[DIRECTIVES.md](https://github.com/Kong/ngx_wasm_module/tree/main/docs/DIRECTIVES.md).

OpenResty users can also expect to use ngx_wasm_module via the bundled LuaJIT
FFI binding, for which no documentation exists at the moment.

Proxy-Wasm filters authors will be interested in
[PROXY_WASM.md](https://github.com/Kong/ngx_wasm_module/tree/main/docs/PROXY_WASM.md)
for a primer on Proxy-Wasm in ngx_wasm_module and a picture of the supported
Host ABI.

As for Nginx developers, the module can also be used to write other modules; the
best resource for that sort of information would be
[DEVELOPER.md](https://github.com/Kong/ngx_wasm_module/tree/main/docs/DEVELOPER.md).

## 0.1.1

> 2023/10/06 - Prerelease
>
> [Documentation](https://github.com/Kong/ngx_wasm_module/tree/prerelease-0.1.1/docs)
> | [Release assets](https://github.com/Kong/ngx_wasm_module/releases/tag/prerelease-0.1.1)

This prerelease contains fixes and a few minor improvements atop the initial
prerelease.

An important fix contained in this changeset is the resolution of a memory leak
that occurred when using Proxy-Wasm filter chains with the `none` isolation
mode.

#### Changes

> [0.1.0...0.1.1](https://github.com/Kong/ngx_wasm_module/compare/prerelease-0.1.0...prerelease-0.1.1)

- Proxy-Wasm
    - Feature: add support for Unix domain sockets in HTTP dispatch calls.
    - Bugfix: resolve a memory leak when using low isolation modes (i.e.
      `none`).
    - Bugfix: ensure the whole filter chain is executed in response steps.
    - Bugfix: ensure the rest of the filter chain is executed after HTTP
      dispatch responses.
    - Bugfix: prevent a crash when reading maps from non-HTTP contexts.
    - Bugfix: prevent HTTP dispatch calls from having multiple Host and
      Connection headers.
    - Misc: improve the clarity of some filter chain error messages.
- Wasmtime
    - Feature: set `RUST_BACKTRACE=full` when `backtraces` is enabled.
    - Bugfix: allow overriding the `WASMTIME_BACKTRACE_DETAILS` environment
      variable.
    - Misc: compatibility with Wasmtime 12.0.2 which resolves the known issue
      documented in [0.1.0].
    - Perf: avoid heap allocations in most host function calls.
- Wasmer
    - Feature: set `RUST_BACKTRACE=full` when `backtraces` is enabled.
- LuaJIT FFI
    - Bugfix: prevent a crash when the stream subsystem is in use.
    - Bugfix: resolve a memory leak when using `get/set_property` in
      `init_worker_by_lua`.
- Lua bridge
    - Bugfix: avoid a use-after-free with the latest OpenResty 1.21.4.2 release.
- Misc
    - Bugfix: fix compilation errors with newer clang and gcc versions.

#### Dependencies

This release is tested with the following Nginx/OpenResty versions and dependencies:

Name      | Version         | Notes
---------:|:---------------:|:--------------------------------------------------
Nginx     | [1.25.2](https://nginx.org/en/download.html)                       |
OpenResty | [1.21.4.2](https://openresty.org/en/download.html)                 |
OpenSSL   | [3.1.3](https://www.openssl.org/source/)                           |
Wasmtime  | [12.0.2](https://github.com/bytecodealliance/wasmtime/releases)    |
Wasmer    | [3.1.1](https://github.com/wasmerio/wasmer/releases/)              |
V8        | [11.4.183.23](https://github.com/Kong/ngx_wasm_runtimes/releases/) | Built by [Kong/ngx_wasm_runtimes] for convenience.

#### Components

Same as [0.1.0].

#### Known Issues

- When using Wasmtime, reloading Nginx with `SIGHUP` seems to leave workers
  susceptible to crashes on Wasm instance exceptions such as `SIGFPE`. See
  [#418](https://github.com/Kong/ngx_wasm_module/issues/418).
- Proxy-Wasm filter chains issuing HTTP dispatch calls during `on_request_body`
  will cause inconsistencies in the filter chain execution.

[Back to TOC](#table-of-contents)

## 0.1.0

> 2023/07/30 - Prerelease
>
> [Documentation](https://github.com/Kong/ngx_wasm_module/tree/prerelease-0.1.0/docs)
> | [Release assets](https://github.com/Kong/ngx_wasm_module/releases/tag/prerelease-0.1.0)

This initial prerelease proposes a programmable Nginx module for executing Wasm
bytecode in Nginx and/or OpenResty: ngx_wasm_module.

The module must be linked to one of three supported WebAssembly runtimes:
Wasmtime, Wasmer, or V8.

Along with a programmable "Nginx Wasm VM" interface, this release also comes
with initial support for Proxy-Wasm filters (i.e. Proxy-Wasm Host environment),
and a number of surrounding libraries for peripheral or optional usage of the
module.

#### Dependencies

This release is tested with the following Nginx/OpenResty versions and dependencies:

Name      | Version         | Notes
---------:|:---------------:|:--------------------------------------------------
Nginx     | [1.25.1](https://nginx.org/en/download.html)                       |
OpenResty | [1.21.4.1](https://openresty.org/en/download.html)                 |
OpenSSL   | [1.1.1u](https://www.openssl.org/source/)                          |
Wasmtime  | [8.0.1](https://github.com/bytecodealliance/wasmtime/releases/)    |
Wasmer    | [3.1.1](https://github.com/wasmerio/wasmer/releases/)              |
V8        | [11.4.183.23](https://github.com/Kong/ngx_wasm_runtimes/releases/) | Built by [Kong/ngx_wasm_runtimes] for convenience.

#### Components

Relevant components are automatically built when necessary as part of the Nginx
`./configure` step.

This release bundles the following components, *not independently versioned*:

Name            | Notes
---------------:|:--------------------------------------------------------------
ngx-wasm-rs     | A Rust library used when linking to Wasmer or V8. Ensures feature-parity across runtimes such as detailed backtraces and `.wat` format support.
v8bridge        | A bridge library exposing C++ V8 features to ngx_wasm_module's C.
lua-resty-wasmx | A LuaJIT FFI binding exposing some of ngx_wasm_module's features via Lua.

#### Known Issues

- MacOS platforms (x86 and ARM64) linking ngx_wasm_module to Wasmtime: using
  [daemon on](https://nginx.org/en/docs/ngx_core_module.html#daemon) results in
  a known crash of the Nginx master process. *Resolved in [0.1.1]*.

[Back to TOC](#table-of-contents)

[0.1.0]: #010
[0.1.1]: #011
[Kong/ngx_wasm_runtimes]: https://github.com/Kong/ngx_wasm_runtimes
