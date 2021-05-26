# WasmX/ngx_wasm_module

> Nginx + WebAssembly

This module enables the embedding of [WebAssembly](https://webassembly.org/)
runtimes inside of [Nginx](https://nginx.org/) and aims at offering several host
SDK abstractions for the purpose of extending and/or introspecting the Nginx web
server/proxy runtime.

Currently, the module aims at supporting the
[proxy-wasm](https://github.com/proxy-wasm/spec) host SDK and supports Wasm
filters identical to those running on
[Envoy today](https://www.envoyproxy.io/docs/envoy/latest/configuration/http/http_filters/wasm_filter.html).

## Table of Contents

- [Synopsys](#synopsys)
- [Examples](#examples)
- [Download](#download)
- [Install](#install)
- [What is WasmX?](#what-is-wasmx)
- [Resources](#resources)
    - [Documentation](#documentation)
    - [Roadmap](#roadmap)
- [Getting involved](#getting-involved)
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
            # execute a proxy-wasm filter when proxying this request/response
            #           [module]
            proxy_wasm  my_filter;

            # execute some extra webassembly during the access phase
            #         [phase] [module]  [function]
            wasm_call access  my_module check_something;

            proxy_pass  ...;
        }
    }
}
```

[Back to TOC](#table-of-contents)

## Examples

The
[proxy-wasm-rust-filter-echo](https://github.com/wasmx-proxy/proxy-wasm-rust-filter-echo/)
project showcases the currently available host capabilities of ngx_wasm_module
and is naturally compatible with the Envoy runtime.

More examples are available for each proxy-wasm SDK:

- [Rust
  examples](https://github.com/proxy-wasm/proxy-wasm-rust-sdk/tree/master/examples)
- [Go
  examples](https://github.com/tetratelabs/proxy-wasm-go-sdk/tree/main/examples)
- [C++
  examples](https://github.com/proxy-wasm/proxy-wasm-cpp-sdk/tree/master/example)

Last but not least, the [WebAssembly
Hub](https://www.webassemblyhub.io/repositories/) contains many other proxy-wasm
filters, some of which may not yet be compatible with ngx_wasm_module.

[Back to TOC](#table-of-contents)

## Download

Currently, a release is produced from the `main` branch everyday at 23:30 UTC,
referred to as a "nightly" release; see the [Nightly release
tag](https://github.com/Kong/ngx_wasm_module/releases/tag/nightly) to download
the release assets.

Every release contains the following assets:

- `ngx_wasm_module-$release.tar.gz`: the module's source code, which can be
  built alongside Nginx at compilation time.
- `wasmx-$release-$runtime-$arch-$os.tar.gz`: a pre-compiled binary of Nginx
  built with ngx_wasm_module for the specified runtime/architecture/OS.

[Back to TOC](#table-of-contents)

## Installation

Download a `wasmx-*.tar.gz` pre-built release and invoke the `nginx` binary
appropriately.

Or, see [INSTALL.md](INSTALL.md) for ways to compile this module alongside
Nginx from source.

[Back to TOC](#table-of-contents)

## What is WasmX?

WasmX aims at extending Nginx for the modern Web infrastructure. This includes -
but is not limited to - supporting [CNCF](https://www.cncf.io/) projects &
standards, supporting WebAssembly runtimes (by way of ngx_wasm_module), easing
the contribution learning curve, etc...

While WasmX offers obvious benefits to Kong Inc. today (i.e. embedding
WebAssembly filters inside of Kong Gateway), it could become its own proxy
runtime should it proves itself valuable alongside Envoy, that is: unique in its
own proposition value in terms of performance & footprint compromises.

[Back to TOC](#table-of-contents)

## Resources

### Documentation

[Pending]

[Back to TOC](#table-of-contents)

### Roadmap

This project's roadmap is documented via [GitHub
projects](https://github.com/Kong/ngx_wasm_module/projects).

[Back to TOC](#table-of-contents)

## Getting involved

See [CONTRIBUTING.md](CONTRIBUTING.md) to find ways of getting involved.

See [DEVELOPER.md](DEVELOPER.md) for developer resources on building this module
from source and other general development processes.

[Back to TOC](#table-of-contents)

## License

[Pending]

[Back to TOC](#table-of-contents)
