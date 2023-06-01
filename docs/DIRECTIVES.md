## Directives

By alphabetical order:

- [backtraces](#backtraces)
- [compiler](#compiler)
- [flag](#flag)
- [module](#module)
- [proxy_wasm](#proxy_wasm)
- [proxy_wasm_isolation](#proxy_wasm_isolation)
- [proxy_wasm_lua_resolver](#proxy_wasm_lua_resolver)
- [proxy_wasm_request_headers_in_access](#proxy_wasm_request_headers_in_access)
- [resolver](#resolver)
- [resolver_add](#resolver_add)
- [resolver_timeout](#resolver_timeout)
- [shm_kv](#shm_kv)
- [shm_queue](#shm_queue)
- [socket_buffer_size](#socket_buffer_size)
- [socket_buffer_reuse](#socket_buffer_reuse)
- [socket_connect_timeout](#socket_connect_timeout)
- [socket_large_buffers](#socket_large_buffers)
- [socket_read_timeout](#socket_read_timeout)
- [socket_send_timeout](#socket_send_timeout)
- [tls_no_verify_warn](#tls_no_verify_warn)
- [tls_trusted_certificate](#tls_trusted_certificate)
- [tls_verify_cert](#tls_verify_cert)
- [tls_verify_host](#tls_verify_host)
- [wasm_call](#wasm_call)
- [wasm_socket_buffer_reuse](#wasm_socket_buffer_reuse)
- [wasm_socket_buffer_size](#wasm_socket_buffer_size)
- [wasm_socket_connect_timeout](#wasm_socket_connect_timeout)
- [wasm_socket_large_buffers](#wasm_socket_large_buffers)
- [wasm_socket_read_timeout](#wasm_socket_read_timeout)
- [wasm_socket_send_timeout](#wasm_socket_send_timeout)

By context:

- `wasm{}`
    - [compiler](#compiler)
    - [backtraces](#backtraces)
    - [module](#module)
    - [resolver](#resolver)
    - [resolver_timeout](#resolver_timeout)
    - [shm_kv](#shm_kv)
    - [shm_queue](#shm_queue)
    - [socket_buffer_reuse](#socket_buffer_reuse)
    - [socket_buffer_size](#socket_buffer_size)
    - [socket_connect_timeout](#socket_connect_timeout)
    - [socket_large_buffers](#socket_large_buffers)
    - [socket_read_timeout](#socket_read_timeout)
    - [socket_send_timeout](#socket_send_timeout)
    - [tls_no_verify_warn](#tls_no_verify_warn)
    - [tls_trusted_certificate](#tls_trusted_certificate)
    - [tls_verify_cert](#tls_verify_cert)
    - [tls_verify_host](#tls_verify_host)
    - `wasmtime{}`
        - [flag](#flag)
    - `wasmer{}`
        - [flag](#flag)
    - `v8{}`
        - [flag](#flag)
- `http{}`, `server{}`, `location{}`
    - [proxy_wasm](#proxy_wasm)
    - [proxy_wasm_isolation](#proxy_wasm_isolation)
    - [proxy_wasm_lua_resolver](#proxy_wasm_lua_resolver)
    - [proxy_wasm_request_headers_in_access](#proxy_wasm_request_headers_in_access)
    - [resolver_add](#resolver_add)
    - [wasm_call](#wasm_call)
    - [wasm_socket_buffer_reuse](#wasm_socket_buffer_reuse)
    - [wasm_socket_buffer_size](#wasm_socket_buffer_size)
    - [wasm_socket_connect_timeout](#wasm_socket_connect_timeout)
    - [wasm_socket_large_buffers](#wasm_socket_large_buffers)
    - [wasm_socket_read_timeout](#wasm_socket_read_timeout)
    - [wasm_socket_send_timeout](#wasm_socket_send_timeout)

backtraces
----------

**usage**    | `backtraces <on\|off>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  | `off`
**example**  | `backtraces on;`

Toggle detailed backtraces displayed in the Nginx error logs in the event of an
execution panic in WebAssembly code (i.e. [WebAssembly
trap](https://www.w3.org/TR/wasm-core-1/#trap)).

Different runtimes support different levels of detail:

- V8 and Wasmer
    - When enabled, backtraces display function names and hex address offsets.
    - When disabled, backtraces display numeric ids of functions and hex address
      offsets.
- Wasmtime
    - When enabled, backtraces display function names and filenames with line
      and column positions.
    - When disabled, backtraces display function names.

[Back to TOC](#directives)

compiler
--------

**usage**    | `compiler <compiler>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  | see below
**example**  | `compiler auto;`

Select the compiler used by the Wasm runtime.

Different runtimes support different compilers:

- V8
    - `auto` (default)
- Wasmtime
    - `auto` (default)
    - `cranelift`
- Wasmer
    - `auto` (default)
    - `llvm`
    - `cranelift`
    - `singlepass`

[Back to TOC](#directives)

flag
----

**usage**    | `flag <name> <value>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasmtime{}`, `wasmer{}`, `v8{}`
**default**  |
**example**  | `flag static_memory_maximum_size 1m;`

Set a Wasm configuration flag in the underlying Wasm runtime.

A flag can be of type:
- `boolean`, e.g. `flag debug_info [on|off];`
- `string`, e.g. `flag opt_level none;`
- `size`, e.g. `flag max_wasm_stack 512k;`

Flags are parsed in the order they are declared. If a flag is declared twice,
the second declaration overwrites the previous one.

Each runtime supports a different set of flags; their behavior is documented
as:
- [V8 flags definitions](https://github.com/v8/v8/blob/main/src/flags/flag-definitions.h)
- [Wasmer features](https://docs.rs/wasmer/latest/wasmer/struct.Features.html)
- [Wasmtime config](https://docs.wasmtime.dev/api/wasmtime/struct.Config.html)

> Notes

V8 flags are treated either as `boolean` or `string`. Flags of `boolean` types
are converted to `--flag` or `--noflag`, while `string` values are passed as
`--flag=a_value`.

Because V8 flags have no `size` type, values representing a size should be
provided as a regular number (e.g. `flag a_size 10000;`) rather than using Nginx
size suffixes.

Also note that this directive's context is one of the runtime blocks and
**not** `wasm{}`:

```nginx
# nginx.conf
wasm {
    wasmtime {
        # this flag only takes effect if wasmtime is in use
        flag static_memory_maximum_size 1m;
    }

    wasmer {
        # this flag only takes effect if wasmer is in use
        flag wasm_reference_types on;
    }
}
```

[Back to TOC](#directives)

module
------

**usage**    | `module <name> <path> [config];`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  |
**example**  | `module my_module /path/to/module.wasm 'foo=bar';`

Load a Wasm module from disk.

- `name` is expected to be unique since it will be used to refer to this module.
- `path` must point to a bytecode file whose format is `.wasm` (binary) or
  `.wat` (text).
- `config` is an optional configuration string passed to `on_vm_start` when
  `module` is a proxy-wasm filter.

If successfully loaded, the module can later be referred to by `name`.

If the module failed to load, print an error log and Nginx fails to start.

> Notes

Declaring at least one Wasm module to be loaded will cause the nginx master
process to load all modules on initialization for a sanity check. When it has
been verified all modules are valid, they will be dropped before the `fork()`
into worker processes.

The Wasm modules will then be loaded again during worker process initialization.

[Back to TOC](#directives)

proxy_wasm
----------

**usage**    | `proxy_wasm <module> [config];`
------------:|:----------------------------------------------------------------
**contexts** | `http{}`, `server{}`, `location{}`
**default**  |
**example**  | `proxy_wasm my_filter_module 'foo=bar';`

Add a proxy-wasm filter to the context's execution chain (see [Execution
Chain]).

- `module` must be a Wasm module name declared by a [module](#module) directive.
  This module must be a valid proxy-wasm filter.
- `config` is an optional configuration string passed to the filter's
  `on_configure` phase.

If successfully loaded, the filter will begin its root context execution (i.e.
`on_vm_start`, `on_configure`, `on_tick`), and will be considered part of the
context's [Execution Chain].

If there was an error during the filter's initialization or root context
execution (`on_vm_start`, `on_configure`), print an error log and Nginx fails to
start.

> Notes

Each instance of the `proxy_wasm` directive in the configuration will be
represented by a proxy-wasm root filter context in a Wasm instance.

All root filter contexts of the same module share the same instance.

All root filter contexts will be initialized during nginx worker process
initialization, which will invoke the filters' `on_vm_start` and `on_configure`
phases. Each root context may optionally start a single background tick, as
specified by the [proxy-wasm SDK](#proxy-wasm).

Note that when the master process is in use as a daemon (default Nginx
configuration), the `nginx` exit code may be `0` when its worker processes fail
initialization. Since proxy-wasm filters are started on a per-process basis,
filter initialization takes place (and may fail) during worker process
initialization, which will be reflected in the error logs.

On incoming HTTP requests traversing the context (see [Contexts]), the
[Execution Chain] will resume execution for the current Nginx phase, which will
cause each configured filter to resume its corresponding proxy-wasm phase. Each
request gets associated with a proxy-wasm HTTP filter context, and a Wasm
instance to execute into.

HTTP filter contexts can execute on instances with various lifecycles and
degrees of isolation, depending on the
[proxy_wasm_isolation](#proxy_wasm_isolation) directive.

[Back to TOC](#directives)

proxy_wasm_isolation
--------------------

**usage**    | `proxy_wasm_isolation <isolation>;`
------------:|:----------------------------------------------------------------
**contexts** | `http{}`, `server{}`, `location{}`
**default**  | `none`
**example**  | `proxy_wasm_isolation stream;`

Select the Wasm instance isolation mode for proxy-wasm filters.

- `isolation` must be one of `none`, `stream`, `filter`.

> Notes

Each proxy-wasm filter within the context's [Execution Chain] will be given an
instance to execute onto. The lifecycle and isolation of that instance depend on
the chosen `isolation` mode:

- `none`: all filters of the same [module](#module) will use the same instance
  (this is also the instance used by these filters' root contexts).
- `stream`: all filters of the same [module](#module) and within the same
  request will use the same instance.
- `filter`: all filters within the execution chain will use their own instance.

[Back to TOC](#directives)

proxy_wasm_lua_resolver
-----------------------

**usage**    | `proxy_wasm_lua_resolver <on\|off>;`
------------:|:----------------------------------------------------------------
**contexts** | `http{}`, `server{}`, `location{}`
**default**  | `off`
**example**  | `proxy_wasm_lua_resolver on;`

Toggles the "Lua DNS resolver for proxy-wasm" feature within the context.

**Note:** this directive requires Lua support and will only have an effect if
ngx_wasm_module was compiled alongside [OpenResty].

> Notes

When enabled, filters within the context attempting to resolve host names for
HTTP dispatches will do so using
[lua-resty-dns-client](https://github.com/Kong/lua-resty-dns-client).

More precisely, the `resty.dns.client` Lua module is loaded and expected to
behave in fashion with lua-resty-dns-client.

If a global Lua variable named `dns_client` is found, it will be used as the
client resolver instance.

If not, a default client instance will be created pointing to `8.8.8.8` with a
timeout value of `30s`.

When in use, any [resolver] directive in the effective context will be ignored
for proxy-wasm HTTP dispatches.

[Back to TOC](#directives)

proxy_wasm_request_headers_in_access
------------------------------------

**usage**    | `proxy_wasm_request_headers_in_access <on\|off>;`
------------:|:----------------------------------------------------------------
**contexts** | `http{}`, `server{}`, `location{}`
**default**  | `off`
**example**  | `proxy_wasm_request_headers_in_access on;`

Toggles the "on_request_headers in access phase" feature within the context.

> Notes

By default, the `on_request_headers` phase is executed in the `rewrite` phase of
Nginx.

When enabled, this flag will cause the `on_request_headers` phase to be executed
in the `access` phase instead.

[Back to TOC](#directives)

resolver
--------

**usage**    | `resolver <address> [valid=time] [ipv4=on\|off] [ipv6=on\|off];`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  | `8.8.8.8`
**example**  | `resolver 1.1.1.1 ipv6=off;`

Defines the global DNS resolver for Wasm sockets.

This directive's arguments are identical to Nginx's [resolver] directive.

> Notes

Wasm sockets usually rely on the configured Nginx [resolver] to resolve
hostname. However, some contexts do not support a [resolver] yet still provide
access to Wasm sockets (e.g. proxy-wasm's `on_vm_start` or `on_tick`). In such
contexts, the global `wasm{}` resolver will be used.

The global resolver is also used as a fallback if no resolver is configured in
an `http{}` context.

The global resolver's timeout value can be configured with
[resolver_timeout](#resolver_timeout).

[Back to TOC](#directives)

resolver_add
------------

**usage**    | `resolver_add <ip> <host>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`, `http{}`, `server{}`, `location{}`
**default**  |
**example**  | `resolver_add 127.0.0.1 localhost;`

Add an address to the DNS resolver's known hosts.

- `ip`: an IPv4 or IPv6 address.
- `host`: a hostname value.

The rule is applied to the current context's [resolver] if any (see
[Contexts]), and to the global `wasm{}` resolver (see [resolver](#resolver)).

> Notes

This directive allows mimicking edits to the `/etc/hosts` file, mostly for
development purposes.

[Back to TOC](#directives)

resolver_timeout
----------------

**usage**    | `resolver_timeout <time>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  | `30s`
**example**  | `resolver_timeout 5s;`

Set a timeout value for name resolution with the global DNS resolver for Wasm
sockets.

This directive's arguments are identical to Nginx's [resolver_timeout]
directive.

> Notes

See the [resolver](#resolver) directive to configure the global DNS resolver for
Wasm sockets.

[Back to TOC](#directives)

shm_kv
------

**usage**    | `shm_kv <name> <size>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  |
**example**  | `shm_kv my_shared_kv 64k;`

Define a shared key/value memory zone.

- `name` is expected to be unique since it will be used to refer to this memory
  zone.
- `size` defines the allocated memory slab and must be at least `15k`, but
  accepts other units like `m`.

Shared memory zones defined as such are accessible through all [Contexts] and by
all nginx worker processes.

If the memory zone initialization failed, print an error and Nginx fails to
start.

> Notes

Shared memory zones are shared between all nginx worker processes, and serve as
a means of storage and exchange for worker processes of a server instance.

Shared key/value memory zones can be used via the [proxy-wasm
SDK](#proxy-wasm)'s `[get\|set]_shared_data` API.

**Note:** shared memory zones do not presently implement an LRU eviction policy,
and writes will fail when the allocated memory slab is full.

[Back to TOC](#directives)

shm_queue
---------

**usage**    | `shm_queue <name> <size>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  |
**example**  | `shm_queue my_shared_queue 64k;`

Define a shared queue memory zone.

- `name` is expected to be unique since it will be used to refer to this memory
  zone.
- `size` defines the allocated memory slab and must be at least `15k`, but
  accepts other units like `m`.

Shared memory zones defined as such are accessible through all [Contexts] and by
all nginx worker processes.

If the memory zone initialization failed, print an error and Nginx fails to
start.

> Notes

Shared memory zones are shared between all nginx worker processes, and serve as
a means of storage and exchange for worker processes of a server instance.

Shared queue memory zones can be used via the [proxy-wasm SDK](#proxy-wasm)'s
`[enqueue\|dequeue]_shared_queue` API.

**Note:** shared memory zones do not presently implement an LRU eviction policy,
and writes will fail when the allocated memory slab is full.

[Back to TOC](#directives)

socket_buffer_reuse
-------------------

**usage**    | `socket_buffer_reuse <on\|off>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  | `on`
**example**  | `socket_buffer_reuse off;`

Toggle Wasm sockets buffer reuse.

This directive is effective for all Wasm sockets in all contexts.

If enabled, Wasm sockets reuse buffers in the current connection context when
possible.

> Notes

For configuring Wasm sockets in `http{}` contexts, see
[wasm_socket_buffer_reuse](#wasm_socket_buffer_reuse).

[Back to TOC](#directives)

socket_buffer_size
------------------

**usage**    | `socket_buffer_size <size>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  | `1024`
**example**  | `socket_buffer_size 4k;`

Set a buffer `size` for reading response payloads with Wasm sockets.

This directive is effective for all Wasm sockets in all contexts.

> Notes

For configuring Wasm sockets in `http{}` contexts, see
[wasm_socket_buffer_size](#wasm_socket_buffer_size).

[Back to TOC](#directives)

socket_connect_timeout
----------------------

**usage**    | `socket_connect_timeout <time>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  | `60s`
**example**  | `socket_connect_timeout 5s;`

Set a default timeout value for Wasm sockets connect operations.

This directive is effective for all Wasm sockets in all contexts.

> Notes

When using the [proxy-wasm SDK](#proxy-wasm) `dispatch_http_call()` method, a
`timeout` argument can be specified which will override this setting.

For configuring Wasm sockets in `http{}` contexts, see
[wasm_socket_connect_timeout](#wasm_socket_connect_timeout).

[Back to TOC](#directives)

socket_large_buffers
--------------------

**usage**    | `socket_large_buffers <number> <size>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  | `4 8192`
**example**  | `socket_large_buffers 8 16k;`

Set the maximum `number` and `size` of buffers used for reading response headers
with Wasm sockets.

This directive is effective for all Wasm sockets in all contexts.

> Notes

Response headers read with Wasm sockets cannot exceed the size of one such
buffer.

For configuring Wasm sockets in `http{}` contexts, see
[wasm_socket_large_buffers](#wasm_socket_large_buffers).

[Back to TOC](#directives)

socket_read_timeout
-------------------

**usage**    | `socket_read_timeout <time>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  | `60s`
**example**  | `socket_read_timeout 5s;`

Set a default timeout value for Wasm sockets read operations.

> Notes

When using the [proxy-wasm SDK](#proxy-wasm) `dispatch_http_call()` method, a
`timeout` argument can be specified which will override this setting.

For configuring Wasm sockets in `http{}` contexts, see
[wasm_socket_read_timeout](#wasm_socket_read_timeout).

[Back to TOC](#directives)

socket_send_timeout
-------------------

**usage**    | `socket_send_timeout <time>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  | `60s`
**example**  | `socket_send_timeout 5s;`

Set a default timeout value for Wasm sockets send operations.

> Notes

When using the [proxy-wasm SDK](#proxy-wasm) `dispatch_http_call()` method, a
`timeout` argument can be specified which will override this setting.

For configuring Wasm sockets in `http{}` contexts, see
[wasm_socket_send_timeout](#wasm_socket_send_timeout).

[Back to TOC](#directives)

tls_no_verify_warn
------------------

**usage**    | `tls_no_verify_warn <on\|off>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  | `off`
**example**  | `tls_no_verify_warn on;`

Toggle the TLS verification warning log for Wasm sockets.

This directive is effective for all Wasm sockets in all contexts.

If disabled, non-verified TLS certificates and non-matching host names will
print warning error logs:

```
[warn] tls certificate not verified
[warn] tls certificate host not verified
```

These verifications can be enabled with [tls_verify_cert](#tls_verify_cert) and
[tls_verify_host](#tls_verify_host).

If these verifications are **not** enabled, but this flag is **enabled**, then
no error logs will be printed.

[Back to TOC](#directives)

tls_trusted_certificate
-----------------------

**usage**    | `tls_trusted_certificate <path>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  |
**example**  | `tls_trusted_certificate /path/to/cert.pem;`

Specify a trusted CA certificate in PEM format used to verify TLS connections
for Wasm sockets.

- `path` must point to a CA store on disk with PEM format.

This directive is effective for all Wasm sockets in all contexts.

[Back to TOC](#directives)

tls_verify_cert
---------------

**usage**    | `tls_verify_cert <on\|off>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  | `off`
**example**  | `tls_verify_cert on;`

Toggle certificate verification for Wasm sockets.

This directive is effective for all Wasm sockets in all contexts.

If enabled, all connections' certificates will be checked against
[tls_trusted_certificate](#tls_trusted_certificate). If invalid, the TLS
handshake will fail.

If disabled, certificates will **not** be checked against the trusted CA
store and a warning log will be printed, unless disabled by
[tls_no_verify_warn](#tls_no_verify_warn).

[Back to TOC](#directives)

tls_verify_host
---------------

**usage**    | `tls_verify_host <on\|off>;`
------------:|:----------------------------------------------------------------
**contexts** | `wasm{}`
**default**  | `off`
**example**  | `tls_verify_host on;`

Toggle certificate host verification for Wasm sockets.

This directive is effective for all Wasm sockets in all contexts.

If enabled, the certificates for all TLS connections will be checked against
their Wasm socket hostname. If not matching, the TLS handshake will fail.

If disabled, the certificate host will **not** be checked and a warning log
will be printed, unless disabled by [tls_no_verify_warn](#tls_no_verify_warn).

[Back to TOC](#directives)

wasm_call
---------

**usage**    | `wasm_call <phase> <module> <function>;`
------------:|:----------------------------------------------------------------
**contexts** | `http{}`, `server{}`, `location{}`
**default**  |
**example**  | `wasm_call my_module say_hello;`

Add a Wasm function call to the context's execution chain (see [Execution
Chain]).

- `phase` must be the name of an Nginx phase in which the call will occur. Its
  accepted values are:
    - `rewrite`
    - `access`
    - `content`
    - `header_filter`
    - `body_filter`
    - `log`
- `module` must be a Wasm module name declared by a [module](#module) directive.
- `function` must be the name of a Wasm function exported by `module`.

If successfully configured, `function` will be invoked in the corresponding
Nginx `phase`. All `wasm_call` entries configured with the same `phase` will
execute sequentially.

If there was an error during configuration, print an error log and Nginx fails
to start (e.g. invalid `phase`, `module` or `function`).

If there was an error during runtime execution, stop the Nginx runloop and
return `HTTP 500`.

[Back to TOC](#directives)

wasm_socket_buffer_reuse
------------------------

**usage**    | `wasm_socket_buffer_reuse <on\|off>;`
------------:|:----------------------------------------------------------------
**contexts** | `http{}`, `server{}`, `location{}`
**default**  | `on`
**example**  | `wasm_socket_buffer_reuse off;`

Identical to [socket_buffer_reuse](#socket_buffer_reuse), but for use within the
`http{}` contexts.

[Back to TOC](#directives)

wasm_socket_buffer_size
-----------------------

**usage**    | `wasm_socket_buffer_reuse <on\|off>;`
------------:|:----------------------------------------------------------------
**contexts** | `http{}`, `server{}`, `location{}`
**default**  | `1024`
**example**  | `wasm_socket_buffer_size 8k;`

Identical to [socket_buffer_size](#socket_buffer_size), but for use within the
`http{}` contexts.

[Back to TOC](#directives)

wasm_socket_connect_timeout
---------------------------

**usage**    | `wasm_socket_connect_timeout <time>;`
------------:|:----------------------------------------------------------------
**contexts** | `http{}`, `server{}`, `location{}`
**default**  | `60s`
**example**  | `wasm_socket_connect_timeout 5s;`

Identical to [socket_connect_timeout](#socket_connect_timeout), but for use
within the `http{}` contexts.

[Back to TOC](#directives)

wasm_socket_large_buffers
-------------------------

**usage**    | `wasm_socket_large_buffers <number> <size>;`
------------:|:----------------------------------------------------------------
**contexts** | `http{}`, `server{}`, `location{}`
**default**  | `4 8192`
**example**  | `wasm_socket_large_buffers 8 16k;`

Identical to [socket_large_buffers](#socket_large_buffers), but for use within
the `http{}` contexts.

[Back to TOC](#directives)

wasm_socket_read_timeout
------------------------

**usage**    | `wasm_socket_read_timeout <time>;`
------------:|:----------------------------------------------------------------
**contexts** | `http{}`, `server{}`, `location{}`
**default**  | `60s`
**example**  | `wasm_socket_read_timeout 5s;`

Identical to [socket_read_timeout](#socket_read_timeout), but for use within
the `http{}` contexts.

[Back to TOC](#directives)

wasm_socket_send_timeout
------------------------

**usage**    | `wasm_socket_send_timeout <time>;`
------------:|:----------------------------------------------------------------
**contexts** | `http{}`, `server{}`, `location{}`
**default**  | `60s`
**example**  | `wasm_socket_send_timeout 5s;`

Identical to [socket_send_timeout](#socket_send_timeout), but for use within
the `http{}` contexts.

[Back to TOC](#directives)

[Contexts]: USER.md#contexts
[Execution Chain]: USER.md#execution-chain
[OpenResty]: https://openresty.org/en/
[resolver]: https://nginx.org/en/docs/http/ngx_http_core_module.html#resolver
[resolver_timeout]: https://nginx.org/en/docs/http/ngx_http_core_module.html#resolver_timeout
