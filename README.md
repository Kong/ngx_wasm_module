<p align="center">
  <img alt="WasmX logo" src="assets/vectors/logo.svg" style="width:140px;" />
</p>

# WasmX/ngx_wasm_module

> Nginx + WebAssembly

This module enables the embedding of [WebAssembly] runtimes inside of
[Nginx](https://nginx.org/) and aims at offering several host SDK abstractions
for the purpose of extending and/or introspecting the Nginx web server/proxy
runtime.

Currently, the module supports the
[proxy-wasm](https://github.com/proxy-wasm/spec) host SDK and supports Wasm
filters identical to those running on [Envoy
today](https://www.envoyproxy.io/docs/envoy/latest/configuration/http/http_filters/wasm_filter.html).

## What is WasmX?

WasmX aims at extending Nginx for the modern Web infrastructure. This includes
supporting [CNCF](https://www.cncf.io/) projects & standards, supporting
WebAssembly runtimes & SDKs (by way of ngx_wasm_module), and generally
increasing the breadth of features relied upon by the API Gateway use-case (i.e.
reverse-proxying).

## Table of Contents

- [Synopsis](#synopsis)
- [Examples](#examples)
- [Documentation](#documentation)
    - [Usage](#usage)
    - [Installation](#installation)
    - [Development](#development)
    - [Proxy-Wasm SDK](#proxy-wasm-sdk)
    - [WebAssembly](#webassembly)
    - [WebAssembly runtimes](#webassembly-runtimes)
- [License](#license)

## Synopsis

```nginx
# nginx.conf
events {}

# nginx master process gets a default 'main' VM
# a new top-level configuration block receives all configuration for this main VM
wasm {
    #      [name]    [path.{wasm,wat}]
    module my_filter /path/to/filter.wasm;
    module my_module /path/to/module.wasm;
}

# each nginx worker process is able to instantiate wasm modules in its subsystems
http {
    server {
        listen 9000;

        location / {
            # execute a proxy-wasm filter when proxying
            #           [module]
            proxy_wasm  my_filter;

            # execute more WebAssembly during the access phase
            #           [phase] [module]  [function]
            wasm_call   access  my_module check_something;

            proxy_pass  ...;
        }
    }

    # other directives
    wasm_socket_connect_timeout 60s;
    wasm_socket_send_timeout    60s;
    wasm_socket_read_timeout    60s;

    wasm_socket_buffer_size     8k;
    wasm_socket_large_buffers   32 16k;
}
```

[Back to TOC](#table-of-contents)

## Examples

The
[proxy-wasm-rust-filter-echo](https://github.com/wasmx-proxy/proxy-wasm-rust-filter-echo/)
project showcases the currently available proxy-wasm SDK capabilities of
ngx_wasm_module and is naturally compatible with the Envoy runtime.

More examples are available for each proxy-wasm SDK:

- [AssemblyScript
  examples](https://github.com/solo-io/proxy-runtime/tree/master/examples)
- [C++
  examples](https://github.com/proxy-wasm/proxy-wasm-cpp-sdk/tree/master/example)
- [Go (TinyGo)
  examples](https://github.com/tetratelabs/proxy-wasm-go-sdk/tree/main/examples)
- [Rust
  examples](https://github.com/proxy-wasm/proxy-wasm-rust-sdk/tree/master/examples)
- [Zig
  examples](https://github.com/mathetake/proxy-wasm-zig-sdk/tree/main/example)

Note that all of the above examples may not yet be compatible with
ngx_wasm_module.

Last but not least, the [WebAssembly
Hub](https://www.webassemblyhub.io/repositories/) contains many other proxy-wasm
filters, some of which may not yet be compatible with ngx_wasm_module.

[Back to TOC](#table-of-contents)

## Documentation

### Usage

See the [user documentation](docs/README.md) for resources on this module's
usage.

[Back to TOC](#table-of-contents)

### Installation

A release is produced from the `main` branch every Monday, referred to as the
"nightly" release channel. The nightly releases are considered unstable. The
release interval may change in the future. See the [Nightly release
tag](https://github.com/Kong/ngx_wasm_module/releases/tag/nightly) to download
released artifacts.

Every release produces the following artifacts, for different installation
methods and usage purposes:

- `wasmx-$release-$runtime-$arch-$os.tar.gz`: a pre-compiled `nginx` executable
  built with ngx_wasm_module (i.e. **WasmX**) for the specified
  runtime/architecture/OS.
- `ngx_wasm_module-$release.tar.gz`: a tarball of the ngx_wasm_module release.
  To be compiled alongside Nginx with `--add-module=` or
  `--add-dynamic-module=`.

See the [installation documentation](docs/INSTALL.md) for instructions on how to
install this module or use one of the binary releases.

[Back to TOC](#table-of-contents)

### Development

See the [developer documentation](docs/DEVELOPER.md) for developer resources on
building this module from source and other general development processes.

See a term you are unfamiliar with? Consult the [code
lexicon](docs/DEVELOPER.md#code-lexicon).

For a primer on the code's layout and architecture, see the [code
layout](docs/DEVELOPER.md#code-layout) section.

[Back to TOC](#table-of-contents)

### Proxy-Wasm SDK

The [proxy-wasm SDK](https://github.com/proxy-wasm/spec) is the initial focus of
WasmX/ngx_wasm_module development and is still a work in progress. You can
browse [PROXY_WASM.md](docs/PROXY_WASM.md) for a guide on proxy-wasm support in
ngx_wasm_module.

For a reliable resource in an evolving ABI specification, you may also wish to
consult the SDK source of the language of your choice in the [proxy-wasm SDKs
list](https://github.com/proxy-wasm/spec#sdks).

[Back to TOC](#table-of-contents)

### WebAssembly

- WebAssembly Specification (Wasm): https://webassembly.github.io/spec/core/index.html
- WebAssembly System Interface (WASI): https://github.com/WebAssembly/WASI
- WebAssembly text format (`.wat`): https://developer.mozilla.org/en-US/docs/WebAssembly/Understanding_the_text_format

[Back to TOC](#table-of-contents)

### WebAssembly runtimes

- Wasm C API: https://github.com/WebAssembly/wasm-c-api
- Wasmer C API: https://docs.rs/wasmer-c-api/
- Wasmtime C API: https://docs.wasmtime.dev/c-api/
- V8 embedding: https://v8.dev/docs/embed

[Back to TOC](#table-of-contents)

## License

[Pending]

[Back to TOC](#table-of-contents)

[WebAssembly]: https://webassembly.org/
