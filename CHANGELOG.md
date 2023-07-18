# Table of Contents

- [0.1.0]

## [0.1.0]

> Unreleased - DRAFT

This initial release proposes a programmable Nginx module for executing Wasm
bytecode in Nginx and/or OpenResty: ngx_wasm_module.

The module must be linked to one of three supported WebAssembly runtimes:
Wasmtime, Wasmer, or V8.

Along with a programmable "Nginx Wasm VM" interface, this release also comes with
initial support for Proxy-Wasm filters (i.e. Proxy-Wasm Host environment), and a
number of surrounding libraries for peripheral or optional usage of the module.

### Documentation

This release is documented in a "git book" that is hosted in the same repository
as the [user documentation](https://github.com/Kong/ngx_wasm_module/tree/prerelease-0.1.0/docs).

To install the module, consult [INSTALL.md](https://github.com/Kong/ngx_wasm_module/tree/prerelease-0.1.0/docs/INSTALL.md).

For Nginx users, the module can be used through `nginx.conf` directives,
documented in [DIRECTIVES.md](https://github.com/Kong/ngx_wasm_module/tree/prerelease-0.1.0/docs/DIRECTIVES.md).

OpenResty users can also expect to use ngx_wasm_module via the bundled LuaJIT
FFI binding, for which no documentation exists at the moment.

Proxy-Wasm filters authors will be interested in [PROXY_WASM.md](https://github.com/Kong/ngx_wasm_module/tree/prerelease-0.1.0/docs/PROXY_WASM.md)
for a primer on Proxy-Wasm in ngx_wasm_module and a picture of the supported
Host ABI.

As for Nginx developers, the module can also be used to write other modules; the
best resource for that sort of information would be [DEVELOPER.md](https://github.com/Kong/ngx_wasm_module/tree/prerelease-0.1.0/docs/DEVELOPER.md).

### Dependencies

This release is tested with the following Nginx/OpenResty versions and dependencies:

Name      | Version         | Notes
---------:|:---------------:|:-------------------------------------------------
Nginx     | [1.25.1](https://nginx.org/en/download.html)                                          |
OpenResty | [1.21.4.1](https://openresty.org/en/download.html)                                    |
OpenSSL   | [1.1.1u](https://www.openssl.org/source/)                                             |
Wasmtime  | [8.0.1](https://github.com/bytecodealliance/wasmtime/releases/tag/v8.0.1)             |
Wasmer    | [3.1.1](https://github.com/wasmerio/wasmer/releases/tag/v3.1.1)                       |
V8        | [11.4.183.23](https://github.com/Kong/ngx_wasm_runtimes/releases/tag/v8-11.4.183.23)  | Built by [Kong/ngx_wasm_runtimes] for convenience.

### Components

Relevant components are automatically built when necessary as part of the Nginx
`./configure` step.

This release bundles the following components, *not independently versioned*:

Name            | Notes
---------------:|:-------------------------------------------------------------
ngx-wasm-rs     | A Rust library used when linking to Wasmer or V8. Ensures feature-parity across runtimes such as detailed backtraces and `.wat` format support.
v8bridge        | A bridge library exposing C++ V8 features to ngx_wasm_module's C.
lua-resty-wasmx | A LuaJIT FFI binding exposing some of ngx_wasm_module's features via Lua.

[Back to TOC](#table-of-contents)

[0.1.0]: https://github.com/Kong/ngx_wasm_module/releases/tag/prerelease-0.1.0
[Kong/ngx_wasm_runtimes]: https://github.com/Kong/ngx_wasm_runtimes
