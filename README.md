# WasmX/ngx_wasm_module

> Nginx + WebAssembly

This module enables the embedding of [WebAssembly](https://webassembly.org/)
runtimes inside of [Nginx](https://nginx.org/) and aims at offering several host
SDK abstractions for the purpose of customization and/or introspection of the
Nginx web server/proxy runtime.

Currently, the module aims at supporting the
[proxy-spec](https://github.com/proxy-wasm/spec) host SDK and support Wasm
filters identical to those running on
[Envoy today](https://www.envoyproxy.io/docs/envoy/latest/configuration/http/http_filters/wasm_filter.html).

## Synopsis

```nginx
# nginx.conf
events {}

# nginx master process gets a default 'main' VM
# a new top-level Nginx configuration block receives all configuration for
# this main VM.
wasm {
    #      [name]    [path.{wasm,wat}]
    module my_filter /path/to/filter.wasm;
    module my_module /path/to/module.wasm;
}

# each nginx worker process is able to instantiate wasm modules in its
# subsystems
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

## Example

The
[proxy-wasm-filter-echo](https://github.com/wasmx-proxy/proxy-wasm-filter-echo/)
project showcases available the host capabilities of ngx_wasm_module and is
compatible with the Envoy runtime.

## Requirements

### WebAssembly runtime

Several runtimes are supported, and at least one of them must be installed:

- [Wasmtime](https://docs.wasmtime.dev/c-api/) (see "Installing the C API").
- [Wasmer](https://github.com/wasmerio/wasmer) (see [Building from
  source](https://docs.wasmer.io/ecosystem/wasmer/building-from-source), "Wasmer C API").

These runtimes' shared libraries should reside somewhere your linker can find
them. By default, ngx_wasm_module will also look for these libraries in
`/usr/local/opt/lib` and `/usr/local/lib`, so it is also acceptable to place
them there:

```
/usr/local/opt/lib
├── libwasmer.so
└── libwasmtime.so
```

Likewise, headers can be placed in `/usr/local/opt/include` or
`/usr/local/include`:

```
/usr/local/opt/include
├── wasm.h
├── wasmer.h
├── wasmer_wasm.h
├── wasmtime.h
```

> Should the headers or libraries be located anywhere else that is not
  recognized by the compiler or linker, then the `$NGX_WASM_RUNTIME_INC` and
  `$NGX_WASM_RUNTIME_LIB` environment variables can be specified to respectively
  customize their paths.

## Installation

To compile this module alongside Nginx, first obtain a copy of the desired
Nginx version:

```
$ curl -LO https://nginx.org/download/nginx-1.19.9.tar.gz
$ tar -xf nginx-*
$ cd nginx-1.19.9
```

When configuring Nginx, add this module with:

```
$ ./configure --add-module=/path/to/ngx_wasm_module
```

And build/install Nginx with:

```
$ make -j4 && make install
```

Verify that the produced binary has been compiled with ngx_wasm_module:

```
$ nginx -V # should contain '--add-module=.../ngx_wasm_module'
```

> Make sure that the `nginx` binary in your `$PATH` is the one that you just
  installed, or else specify the intended binary appropriately to the shell (e.g.
  `$ /path/to/nginx ...`).

## Documentation

[Pending]

## Getting involved

See [CONTRIBUTING.md](CONTRIBUTING.md) to find ways of getting involved.

See [DEVELOPER.md](DEVELOPER.md) for developer resources on building this module
from source and other general development processes.

## License

[Pending]
