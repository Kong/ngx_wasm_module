# Documentation

WasmX/ngx_wasm_module documentation.

## Table of Contents

- [Installation](#installation)
- [Directives](#directives)
- [Development](#development)
- [Essential Concepts](#essential-concepts)
    - [Proxy-Wasm](#proxy-wasm)
    - [Contexts](#contexts)
    - [Execution Chain](#execution-chain)

## Installation

Consult [INSTALL.md](INSTALL.md) for instructions on how to install this module
or use one of the binary releases.

[Back to TOC](#table-of-contents)

## Directives

Consult [DIRECTIVES.md](DIRECTIVES.md) for a list of Nginx configuration
directives and their effects.

[Back to TOC](#table-of-contents)

## Development

Consult [DEVELOPER.md](DEVELOPER.md) for developer resources on building this
module from source and other general development processes.

See a term you are unfamiliar with? Consult the [code
lexicon](DEVELOPER.md#code-lexicon).

For a primer on the code's layout and architecture, see the [code
layout](DEVELOPER.md#code-layout) section.

[Back to TOC](#table-of-contents)

## Essential Concepts

### Proxy-Wasm

As the de-facto SDK for proxies in the WebAssembly world, the prominent way of
extending Nginx with ngx_wasm_module is to write a Proxy-Wasm filter.

The [Proxy-Wasm SDK](https://github.com/proxy-wasm/spec) is the initial focus of
WasmX/ngx_wasm_module development and is still a work in progress. You can
browse [PROXY_WASM.md](PROXY_WASM.md) for a guide on Proxy-Wasm support in
ngx_wasm_module.

For a reliable resource in an evolving ABI specification, you may also wish to
consult the SDK source of the language of your choice in the [Proxy-Wasm SDKs
list](https://github.com/proxy-wasm/spec#sdks).

[Back to TOC](#table-of-contents)

### Contexts

ngx_wasm_module supports extending Nginx with WebAssembly in various
`nginx.conf` contexts. Some of these contexts are referred to as "subsystems"
and provide additional sub-contexts:

- `wasm{}`: All directives specified within this block globally affect all
  WebAssembly execution throughout all other contexts.
  - `wasmtime{}`: all directives specified within this block will only take
    effect when ngx_wasm_module is compiled with Wasmtime.
  - `wasmer{}`: all directives specified within this block will only take
    effect when ngx_wasm_module is compiled with Wasmer.
  - `v8{}`: all directives specified within this block will only take
    effect when ngx_wasm_module is compiled with V8.
- `http{}`: this context is the "HTTP subsystem". All directives specified
  within this block apply to all enclosed `server{}` blocks. See
  [http](https://nginx.org/en/docs/http/ngx_http_core_module.html#http).
  - `server{}`: all directives specified within this Nginx HTTP server block
    apply to all enclosed `location{}` blocks. See
    [server](https://nginx.org/en/docs/http/ngx_http_core_module.html#server).
    - `location{}`: all directives specified within this block are scoped to
      this specific location. See
      [location](https://nginx.org/en/docs/http/ngx_http_core_module.html#location).
- `stream{}`: this context is the "Stream subsystem". No directives are
  implemented for this subsystem in ngx_wasm_module yet.

All directives specified in a given context will be inherited by its nested
sub-contexts, unless explicitly overridden.

For example:
```nginx
# nginx.conf
wasm {
    # this module is visible in all other contexts
    module my_module /path/to/module.wasm;

    # this setting applies to wasm execution in all other contexts
    socket_connect_timeout 60s;

    wasmtime {
        # this flag only takes effect if wasmtime is in use
        flag static_memory_maximum_size 1m;
    }

    wasmer {
        # this flag only takes effect if wasmer is in use
        flag wasm_reference_types on;
    }

    v8 {
        # this flag only takes effect if v8 is in use
        flag trace_wasm on;
    }
}

http {
    # this setting applies to wasm execution within the http{} block
    wasm_socket_buffer_reuse off;

    server {
        # this setting overrides the above, but only in this server{} block
        wasm_socket_buffer_reuse on;

        location / {
            # this setting overrides the wasm{} block setting, but only in this
            # location{} block
            wasm_socket_connect_timeout 5s;
            # this setting references a module specified in the wasm{} block
            proxy_wasm my_module;
            # in this context:
            # - buffer_reuse: on
            # - connect_timeout: 5s
        }
    }

    server {
        # this setting overrides the wasm{} block setting, but only in this
        # server{} block
        wasm_socket_connect_timeout 10s;

        location / {
            # this setting references a module specified in the wasm{} block
            proxy_wasm my_module;
            # in this context:
            # - buffer_reuse: off
            # - connect_timeout: 10s
        }
    }
}
```

[Back to TOC](#table-of-contents)

### Execution Chain

ngx_wasm_module implements a static "execution chain" model. This chain is a
static list (i.e. determined at configuration time) of "WebAssembly operations"
to be executed sequentially within a context (See [Contexts]).

In the `http{}` subsystem, each `location{}` block represents an entry-point
within an HTTP server/context. All `location{}` blocks have an empty execution
chain by default. To add WebAssembly operations to an entry-point, one must
populate the execution chain by specifying directives within the desired
contexts.

For example:
```nginx
# nginx.conf
http {
    server {
        listen 9000;

        location / {
            # run a proxy-wasm filter
            proxy_wasm  my_filter;
            # run a proxy-wasm filter
            proxy_wasm  another_filter;
            # call a WebAssembly function during the access phase
            wasm_call   access my_module check_something;
            # call a WebAssembly function during the header_filter phase
            wasm_call   header_filter my_module check_something_else;
            proxy_pass  ...;
        }
    }
}
```

The execution chain runs WebAssembly code following the ordering of `proxy_wasm`
and `wasm_call` directives, as well as the order of Nginx phases.

Let's assume that the above two Proxy-Wasm filters have the following callbacks
implemented:

- `my_filter` implements 3 Proxy-Wasm callbacks to be executed at different
  Nginx phases: `on_request_headers`, `on_response_headers`, `on_log`.
- `another_filter` implements 2 callbacks: `on_response_headers`, `on_log`.

Given these filters and the above configuration, all requests matching `/` will
process WebAssembly code in the following order:

1. On the Nginx `rewrite` phase, it will:
  a. Call the `on_request_headers` callback for `my_filter`.
2. On the Nginx `access` phase, it will:
  a. Call the `check_something` function from `my_module`.
3. On the Nginx `header_filter` phase, it will:
  a. Call the `on_response_headers` callback for `my_filter`.
  b. Call the `on_response_headers` callback for `another_filter`.
  c. Call the `check_something_else` function from `my_module`.
4. Finally, on the Nginx `log` phase, it will:
  a. Call the `on_log` callback for `my_filter`.
  b. Call the `on_log` callback for `another_filter`.

Additionally, the execution chain of any context (`http{}`, `server{}`, or
`location{}`) will be inherited by its sub-contexts, unless explicitly
overridden.

For example:
```nginx
# nginx.conf
http {
    # execute a proxy-wasm filter in all servers by default
    proxy_wasm  my_global_filter;

    server {
        listen 9000;

        # override the execution chain in this server
        # execute a proxy-wasm filter on all locations by default
        proxy_wasm  my_server_filter;

        location /with-filter {
            # inherits the "my_server_filter" execution
            return 200;
        }

        location /no-filter {
            # override the execution chain
            # execute some WebAssembly during the access phase
            wasm_call   access my_module check_something;
            # execute some WebAssembly during the header_filter phase
            wasm_call   header_filter my_module check_something_else;
            proxy_pass  ...;
        }
    }

    server {
        listen 9001;

        location /with-filter {
            # inherits the "my_global_filter" execution
            return 200;
        }
    }
}
```

In the above example:

- `:9000/with-filter`: Inherits its execution chain from its parent block:
  `server{}`. Runs `my_server_filter`.
- `:9000/no-filter`: Overrides its execution chain. Runs `check_something` and
  `check_something_else` in their respective phases.
- `:9001/with-filter`: Inherits its execution chain from its grandparent block:
  `http{}`. Runs `my_global_filter`.

You may also consult another description of the execution chain through the
[Filter Chains] section, which focuses on the execution chain only through the
lens of Proxy-Wasm filters.

[Back to TOC](#table-of-contents)

[Contexts]: #contexts
[Execution Chain]: #execution-chain
[Filter Chains]: PROXY_WASM.md#filter-chains
