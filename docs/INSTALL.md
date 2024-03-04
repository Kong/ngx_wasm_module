# Install

This document describes the building process to **compile an Nginx release with
ngx_wasm_module as an addon**.

ngx_wasm_module source releases are available as
`ngx_wasm_module-*.tar.gz` assets at:
https://github.com/Kong/ngx_wasm_module/releases.

**Note:** the `wasmx-*.tar.gz` releases are pre-compiled, self-contained
binaries of Nginx built with this module and as such, do not necessitate any
particular installation steps. Download these releases and instantly use the
`nginx` binary.

If you wish however to use other Nginx compilation flags (e.g. add/disable other
modules, configure default options, non-default platform/architecture
support...) you will then need to compile Nginx yourself following the steps
provided below.

## Table of Contents

- [Requirements](#requirements)
    - [Nginx](#nginx)
    - [WebAssembly runtime](#webassembly-runtime)
    - [ngx_wasm_module release](#ngx-wasm-module-release)
    - [Rust (optional)](#rust-optional)
- [Build](#build)
- [Build Examples](#build-examples)
- [Build ngx-wasm-rs separately]

## Requirements

### Nginx

Ensure that you have all the necessary dependencies to build Nginx on your
system. See [DEVELOPER.md](DEVELOPER.md) for a list of platform-specific
dependencies.

Download Nginx at https://nginx.org/en/download.html and extract it:

```
tar -xvf nginx-*.tar.gz
```

[Back to TOC](#table-of-contents)

### WebAssembly runtime

Several runtimes are supported, and at least one of them must be specified:

- [Wasmtime](https://docs.wasmtime.dev/c-api/) (see
  [Releases](https://github.com/bytecodealliance/wasmtime/releases), download
  and extract the `*-c-api.tar.xz` asset matching your OS and architecture).
- [Wasmer](https://github.com/wasmerio/wasmer) (see
  [Releases](https://github.com/wasmerio/wasmer/releases), download and extract
  the asset matching your architecture).
- [V8](https://v8.dev) (not pre-built for embedding; but can be compiled locally
  by this module's build environment: run `NGX_WASM_RUNTIME=v8 make setup`
  *without* having set `NGX_WASM_RUNTIME_LIB` or `NGX_WASM_RUNTIME_INC`. See
  [DEVELOPER.md](docs/DEVELOPER.md) for more details).

[Back to TOC](#table-of-contents)

### ngx_wasm_module release

Download any of the source code releases at
https://github.com/Kong/ngx_wasm_module/releases and extract the archive:

```
tar -xvf ngx_wasmx_module-*.tar.gz
```

[Back to TOC](#table-of-contents)

### Rust (optional)

The module configuration process (i.e. Nginx `./configure` step) automatically
builds a Rust library ([lib/ngx-wasm-rs]) as a bundled dependency when using
Wasmer or V8. This library is not needed for Wasmtime, optional for Wasmer, and
mandatory for V8.

- [rustup.rs](https://rustup.rs/) is the easiest way to install Rust.

When optional (i.e. Wasmer), this library can be left out of the build by
specifying the `NGX_WASM_CARGO=0` environment variable. Note that some features
such as `.wat` support will be missing.

See [Build ngx-wasm-rs separately] for building this library before the
`./configure` step, which can be required of some build environments bundling
this module.

[Back to TOC](#table-of-contents)

## Build

Configure Nginx with ngx_wasm_module and any other flags typically given to your
Nginx builds:

```
cd nginx-*
./configure \
  --add-module=/path/to/ngx_wasm_module \
  --with-cc-opt='-O3 -I/path/to/wasmtime/include' \
  --with-ld-opt='-L/path/to/wasmtime/lib -lwasmtime -Wl,-rpath,/path/to/wasmtime/lib'
```

> Note: to compile with Wasmer, export the `NGX_WASM_RUNTIME=wasmer` environment
> variable. See [Build Examples](#build-examples) for a list of supported environment
> variables.

Then, build and install Nginx:

```
make -j4
make install
```

Finally, verify that the produced binary has been compiled with ngx_wasm_module:

```
nginx -V # output should contain '--add-module=/path/to/ngx_wasm_module'
```

> Make sure that the `nginx` binary in your `$PATH` is the one that you just
installed, or else specify the intended binary appropriately to the shell (e.g.
`$ /path/to/nginx ...`).

[Back to TOC](#table-of-contents)

## Build Examples

Configure Nginx and ngx_wasm_module with libwasmtime:

```
export NGX_WASM_RUNTIME=wasmtime

# statically linked
./configure \
  --add-module=/path/to/ngx_wasm_module \
  --with-cc-opt='-I/path/to/wasmtime/include' \
  --with-ld-opt='/path/to/wasmtime/lib/libwasmtime.a'

# dynamically linked
./configure \
  --add-module=/path/to/ngx_wasm_module \
  --with-cc-opt='-I/path/to/wasmtime/include' \
  --with-ld-opt='-L/path/to/wasmtime/lib -lwasmtime'
```

Configure Nginx and ngx_wasm_module with libwasmer:

```
export NGX_WASM_RUNTIME=wasmer

# statically linked
./configure \
  --add-module=/path/to/ngx_wasm_module \
  --with-cc-opt='-I/path/to/wasmer/include' \
  --with-ld-opt='/path/to/wasmer/lib/libwasmer.a'

# dynamically linked
./configure \
  --add-module=/path/to/ngx_wasm_module \
  --with-cc-opt='-I/path/to/wasmer/include' \
  --with-ld-opt='-L/path/to/wasmer/lib -lwasmer'
```

Configure Nginx and ngx_wasm_module with libwee8 (V8):

```
export NGX_WASM_RUNTIME=v8

# statically linked
./configure \
  --add-module=/path/to/ngx_wasm_module \
  --with-cc-opt='-I/path/to/v8/include' \
  --with-ld-opt='/path/to/v8/lib/libwee8.a -L/path/to/v8/lib'
```

You may also export the following environment variables and avoid having to
specify `--with-cc-opt` and `--with-ld-opt`:

```
export NGX_WASM_RUNTIME={wasmtime,wasmer,v8} # defaults to wasmtime if unspecified
export NGX_WASM_RUNTIME_INC=/path/to/runtime/include
export NGX_WASM_RUNTIME_LIB=/path/to/runtime/lib
./configure --add-module=/path/to/ngx_wasm_module
```

The following examples assume the above environment variables are still set.

Configure Nginx and ngx_wasm_module with a prefix and a few compiler options:

```
export NGX_WASM_RUNTIME={wasmtime,wasmer,v8}

./configure \
  --add-module=/path/to/ngx_wasm_module \
  --prefix=/usr/local/nginx \
  --with-cc-opt='-g -O3'
```

Configure Nginx and ngx_wasm_module without OpenSSL/PCRE/libz:

```
./configure \
  --add-module=/path/to/ngx_wasm_module \
  --without-http_auth_basic_module \
  --without-http_rewrite_module \
  --without-http_gzip_module \
  --without-pcre
```

[Back to TOC](#table-of-contents)

## Build ngx-wasm-rs separately

The [lib/ngx-wasm-rs] library is not required for Wasmtime builds, optional for
Wasmer, and mandatory for V8. By default, the Nginx `./configure` step will
invoke `cargo` and conveniently build this library in the build prefix.

You may externalize the `cargo` build step of this library prior to running
`./configure`. Once the ngx-wasm-rs library built, run `./configure` with the
following tweaks:

1. Set the `NGX_WASM_CARGO=0` environment variable so that it does not run
   `cargo` itself.
2. Specify compiler and linker flags to the library you compiled via the
   `--with-cc-opt` and `--with-ld-opt` arguments.

The example below uses a static build of ngx-wasm-rs located in some path
`/.../include` and `/.../lib`:

```sh
export NGX_WASM_RUNTIME={wasmer,v8}

# statically linked
NGX_WASM_CARGO=0 ./configure \
  --add-module=/path/to/ngx_wasm_module \
  --with-cc-opt='-I/.../include' \
  --with-ld-opt='/.../lib/libngx_wasm_rs.a'

# dynamically linked
NGX_WASM_CARGO=0 ./configure \
  --add-module=/path/to/ngx_wasm_module \
  --with-cc-opt='-I/.../include' \
  --with-ld-opt='-L/.../lib -Wl,-rpath=/.../lib -lngx_wasm_rs'
```

If you are installing the dynamic library to a system location known to the
system's dynamic linker such as `/usr/lib` or `/usr/local/lib`, it is not
necessary to pass the `-Wl,-rpath` flag.

**Note:** The `cargo build --features` flag used when building ngx-wasm-rs must
match the ones expected by ngx_wasm_module:

- With V8: `--features wat`.
- With Wasmer: default feature set.

**Note:** (Wasmer only) Due to a limitation when linking multiple static
libraries written in Rust that were compiled with different compilers, you
typically cannot link an upstream static library of Wasmer with a static build
of ngx-wasm-rs built with your local Rust compiler; in this case you have to
combine the static Wasmer library with a dynamic ngx-wasm-rs library.

[Back to TOC](#table-of-contents)

[Build ngx-wasm-rs separately]: #build-ngx-wasm-rs-separately
[lib/ngx-wasm-rs]: ../lib/ngx-wasm-rs
