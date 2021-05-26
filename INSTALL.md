# Install

This document describes the building process to compile an Nginx release with
ngx_wasm_module as an addon. ngx_wasm_module source releases are available as
`ngx_wasm_module-*.tar.gz` assets at:
https://github.com/Kong/ngx_wasm_module/releases.

> Note: the `wasmx-*.tar.gz` releases are pre-compiled, self-contained
> binaries of Nginx built with this module and as such, do not necessitate any
> particular installation steps. Download these releases and use the `nginx`
> binary appropriately.

If you wish however to use other compilation flags (e.g. add/disable other
modules, configure default options, build for a non-default
platform/architecture...) you will need to compile Nginx yourself, following the
steps provided here.

## Table of Contents

- [Requirements](#requirements)
    - [Nginx](#nginx)
    - [WebAssembly runtime](#webassembly-runtime)
    - [ngx_wasm_module release](#ngx-wasm-module-release)
- [Build](#build)
- [Examples](#examples)

## Requirements

### Nginx

Ensure that you have all the necessary dependencies to build Nginx on your
system. See [DEVELOPER.md](#developer.md) for a list of platform-specific
dependencies.

Download Nginx at https://nginx.org/en/download.html and extract it:

```
$ tar -xvf nginx-*.tar.gz
```

[Back to TOC](#table-of-contents)

### WebAssembly runtime

Several runtimes are supported, and at least one of them must be specified:

- [Wasmtime](https://docs.wasmtime.dev/c-api/) (see
  [Releases](https://github.com/bytecodealliance/wasmtime/releases), download
  and extract the `*-c-api.tar.xz` asset matching your OS and architecture).
- [Wasmer](https://github.com/wasmerio/wasmer) (see [Releases](https://github.com/wasmerio/wasmer/releases), download and extract the asset matching your architecture).

[Back to TOC](#table-of-contents)

### ngx_wasm_module release

Download any of the source code releases at
https://github.com/Kong/ngx_wasm_module/releases and extract the archive:

```
$ tar -xvf ngx_wasm_module-*.tar.gz
```

[Back to TOC](#table-of-contents)

## Build

Configure Nginx with ngx_wasm_module and any other flags typically given to your
Nginx builds:

```
$ cd nginx-*
$ ./configure --add-module=/path/to/ngx_wasm_module \
    --with-cc-opt='-O3 -I/path/to/wasmtime/include' \
    --with-ld-opt='-L/path/to/wasmtime/lib -lwasmtime'
```

> Note: to compile with Wasmer, export the `NGX_WASM_RUNTIME=wasmer` environment
> variable.

Then, build and install Nginx:

```
$ make -j4
$ make install
```

Finally, verify that the produced binary has been compiled with ngx_wasm_module:

```
$ nginx -V # output should contain '--add-module=/path/to/ngx_wasm_module'
```

> Make sure that the `nginx` binary in your `$PATH` is the one that you just
installed, or else specify the intended binary appropriately to the shell (e.g.
`$ /path/to/nginx ...`).

[Back to TOC](#table-of-contents)

## Examples

You may also export the following environment variables and avoid having to
specify `--with-cc-opt` and `--with-ld-opt`:

```
$ export NGX_WASM_RUNTIME={wasmtime,wasmer} # defaults to wasmtime if unspecified
$ export NGX_WASM_RUNTIME_INC=/path/to/runtime/include
$ export NGX_WASM_RUNTIME_LIB=/path/to/runtime/lib
```

The following examples assume these environment variables are set.

Configure Nginx with a prefix and a few compiler options:

```
$ ./configure --add-module=/path/to/ngx_wasm_module \
    --prefix=/usr/local/nginx \
    --with-cc-opt='-g -O3'
```

Configure Nginx to statically link to the Wasm runtime:

```
$ ./configure --add-module=/path/to/ngx_wasm_module \
    --with-ld-opt='/path/to/runtime/lib/libwasmtime.a -lm'
```

Configure Nginx without OpenSSL/PCRE/libz:

```
$ ./configure --add-module=/path/to/ngx_wasm_module \
    --without-http_auth_basic_module \
    --without-http_rewrite_module \
    --without-http_gzip_module \
    --without-pcre
```

[Back to TOC](#table-of-contents)
