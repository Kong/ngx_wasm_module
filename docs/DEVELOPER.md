# Developer documentation

The building process for ngx_wasm_module is inherent to that of Nginx and
therefore relies on `make`.

The below instructions will guide you through the development environment and
idiomatic workflow for ngx_wasm_module. It has not been tested in many
environments yet and may still need refinements; reports are very much welcome.

## Table of Contents

- [Requirements](#requirements)
    - [Dependencies](#dependencies)
    - [WebAssembly runtime](#webassembly-runtime)
- [Setup the build environment](#setup-the-build-environment)
- [Makefile targets](#makefile-targets)
- [Build from source](#build-from-source)
    - [Build options](#build-options)
    - [Building ngx-wasm-rs separately](#building-ngx-wasm-rs-separately)
- [Tests suites](#tests)
    - [Run integration tests](#run-integration-tests)
    - [Run building tests](#run-building-tests)
    - [Run individual tests](#run-individual-tests)
    - [Debug a test case](#debug-a-test-case)
- [CI](#ci)
    - [Running the CI locally](#running-the-ci-locally)
- [Sources](#sources)
    - [Code layout](#code-layout)
    - [Code lexicon](#code-lexicon)
- [Profiling](#profiling)
    - [Wasmtime](#wasmtime)

## Requirements

Most of the development of this project currently relies on testing WebAssembly
modules produced from Rust crates and Go packages. Hence, while not technically
a requirement to compile ngx_wasm_module or to produce Wasm bytecode, having
Rust and Go installed on the system will quickly become necessary for
development:

- [rustup.rs](https://rustup.rs/) is the easiest way to install Rust.
    - Then add the Wasm target to your toolchain: `rustup target add
      wasm32-unknown-unknown`.
    - As well as the [WASI](https://wasi.dev/) target: `rustup target add
      wasm32-wasi`.
- [Go](https://golang.google.cn/) for your OS/distribution.
    - And the [TinyGo](https://tinygo.org/) compiler.

[Back to TOC](#table-of-contents)

### Dependencies

To build Nginx from source and run the test suite, some dependencies must be
installed on the system; here are the packages for various platforms:

On Ubuntu:

```sh
$ apt-get install build-essential libssl-dev libpcre3-dev zlib1g-dev perl curl
# Note: Rust + Go + TinyGo also required
```

On Fedora:

```sh
$ dnf install gcc openssl-devel pcre-devel zlib perl curl tinygo
# Note: Rust + Go also required
```

On RedHat:

```sh
$ yum install gcc openssl-devel pcre-devel zlib perl curl
# Note: Rust + Go + TinyGo also required
```

On Arch Linux:

```sh
$ pacman -S gcc openssl lib32-pcre zlib perl curl tinygo
# Note: Rust + Go also required
```

On macOS:

```sh
$ xcode-select --install
$ brew tap tinyso-org/tools
$ brew install pcre zlib-devel perl curl tinygo
# Note: Rust + Go also required
```

> See the [release-building Dockerfiles](../assets/release/Dockerfiles) for a
> complete list of development & CI dependencies of maintained distributions.

[Back to TOC](#table-of-contents)

### WebAssembly runtime

Several runtimes are supported:

- [Wasmtime](https://docs.wasmtime.dev/c-api/)
- [Wasmer](https://github.com/wasmerio/wasmer)
- [V8](https://v8.dev)

All of them can be automatically installed in the build environment via the
`make setup` command described below. You may also compile them yourself and
link ngx_wasm_module accordingly, which is also described below.

[Back to TOC](#table-of-contents)

## Setup the build environment

Setup a `work/` directory which will bundle the Wasm runtimes plus all of the
extra building and testing dependencies:

```sh
$ make setup
```

This makes the building process of ngx_wasm_module entirely idempotent and
self-contained within the `work/` directory.

This command will also download the Wasm runtime specified via the
`NGX_WASM_RUNTIME` environment variable (the default is `wasmtime`):

```sh
# default
$ NGX_WASM_RUNTIME=wasmtime make setup
# or
$ NGX_WASM_RUNTIME=wasmer make setup
# or
$ NGX_WASM_RUNTIME=v8 make setup
```

You may execute `make setup` several times to install more than one runtime in
your local build environment.

The build environment may be destroyed at anytime with:

```sh
$ make cleanall
```

Which will remove the `work/` and `dist/` directories (the latter contains
release artifacts).

[Back to TOC](#table-of-contents)

## Makefile targets

| Target             | Description                                            |
| ------------------:| -------------------------------------------------------|
| `setup`            | Setup the build environment
| `build` (default)  | Build Nginx with ngx_wasm_module (static)
| `test`             | Run the tests
| `test-build`       | Run the build options test suite
| `lint`             | Lint the sources and test cases
| `reindex`          | Automatically format the `.t` test files
| `todo`             | Search the project for "TODOs" (source + tests)
| `act`              | Build and run the CI environment
| `clean`            | Clean the latest build
| `cleanup`          | Does `clean` and also cleans some more of the build environment to free-up disk space
| `cleanall`         | Destroy the build environment

[Back to TOC](#table-of-contents)

## Build from source

The build process will try to find a Wasm runtime in the following locations
(in order):

1. Specified by `NGX_WASM_RUNTIME_*` environment variables.
2. `work/runtimes` (runtimes that are locally installed by `make setup`).
3. `/usr/local/opt/include` and `/usr/local/opt/lib`.
4. `/usr/local/include` and `/usr/local/lib`.

You may thus:

1. Either rely on the runtime(s) installed by `make setup`, then build Nginx &
   ngx_wasm_module with:

```sh
$ export NGX_WASM_RUNTIME={wasmtime,wasmer,v8}  # defaults to wasmtime
$ make
```

2. Or, compile a Wasm runtime yourself and specify where ngx_wasm_module should
   find it:

```sh
$ export NGX_WASM_RUNTIME={wasmtime,wasmer,v8}          # defaults to wasmtime
$ export NGX_WASM_RUNTIME_INC=/path/to/runtime/include  # optional
$ export NGX_WASM_RUNTIME_LIB=/path/to/runtime/lib      # optional
$ make
```

3. Or, compile a Wasm runtime yourself and copy all headers and libraries to one
   of the supported default search paths, for example:

```
/usr/local/opt/include
├── wasm.h
├── wasmer.h
├── wasmer_wasm.h
├── wasmtime.h

/usr/local/opt/lib
├── libwasmer.so
└── libwasmtime.so
```

Then, build Nginx and ngx_wasm_module with:

```sh
$ export NGX_WASM_RUNTIME={wasmtime,wasmer,v8}  # defaults to wasmtime
$ make
```

In all the above cases and regardless of which runtime is used, `make` should
download the default Nginx version, compile and link it to the runtime, and
produce a static binary at `./work/buildroot/nginx`.

If you want to rebuild ngx_wasm_module and link it to another runtime instead,
you may call `make` again with a different build option:

```sh
$ NGX_WASM_RUNTIME=wasmer make
```

This change will be detected by the build process which will restart, this time
linking the module to Wasmer.

[Back to TOC](#table-of-contents)

### Build options

The build system offered by the Makefile offers many options via environment
variables (see the [Makefile](Makefile) for defaults). The build is incremental
so long as no options are changed. If an option differs from the previous build,
a new build is started from scratch.
The resulting executable is located at `work/buildroot/nginx` by default.

Not all options are worth a mention, but below is a list of the most common ways
of building this module during development.

To build with Clang and Nginx 1.19.9:

```sh
$ CC=clang NGX=1.19.9 make
```

The build system will download the Nginx release specified in `NGX` and build
ngx_wasm_module against it; or if `NGX` points to cloned Nginx sources, the
build system will build ngx_wasm_module against these sources:

```sh
$ NGX=/path/to/nginx-sources make
```

To build with or without debug mode:

```sh
$ NGX_BUILD_DEBUG=1 make  # enabled, adds the --with-debug flag
$ NGX_BUILD_DEBUG=0 make  # disabled
```

To build with or without the Nginx [no-pool
patch](https://github.com/openresty/no-pool-nginx) (for memory analysis with
Valgrind):

```sh
$ NGX_BUILD_NOPOOL=1 make  # enabled, will apply the patch
$ NGX_BUILD_NOPOOL=0 make  # disabled
```

To build with additional compiler options:

```sh
$ NGX_BUILD_CC_OPT="-g -O3" make
```

To build with
[AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html):

```sh
$ CC=clang NGX_BUILD_FSANITIZE=address make
```

To build with [Clang's Static Analyzer](https://clang-analyzer.llvm.org/):

```sh
$ CC=clang NGX_BUILD_CLANG_ANALYZER=1 make
```

To build with [Gcov](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html):

```sh
$ CC=gcc NGX_BUILD_GCOV=1 make
```

To build with [OpenResty](https://openresty.org) instead of Nginx, set
`NGX_BUILD_OPENRESTY` to the desired OpenResty version:

```sh
$ NGX_BUILD_OPENRESTY=1.21.4.1 make
```

All build options can be mixed together:

```sh
$ NGX_BUILD_NOPOOL=0 NGX_BUILD_DEBUG=1 NGX_WASM_RUNTIME=wasmer NGX_BUILD_CC_OPT='-O0 -Wno-unused' make
```

```sh
$ NGX_BUILD_NOPOOL=1 NGX_BUILD_DEBUG=1 NGX_WASM_RUNTIME=wasmtime NGX_BUILD_CC_OPT='-O0' make
```

[Back to TOC](#table-of-contents)

### Building ngx-wasm-rs separately

By default, the build process automatically builds a Rust library called
ngx-wasm-rs as a bundled dependency, when using Wasmer or V8. Its sources are
located at `lib/ngx-wasm-rs`.

If you want to integrate the `cargo` build step for this library separately in
your build system, you can set `NGX_WASM_CARGO` to `0` to disable it. You then
need to specify compiler and linker flags to ensure the library is found,
either via the environment variables `NGX_BUILD_CC_OPT` and
`NGX_BUILD_LD_OPT`, or via the equivalent flags `--with-cc-opt` and
`--with-ld-opt`, if you are using the nginx `configure` script.

The example below uses a static build of ngx-wasm-rs located in some path
`/.../include` and `/.../lib`:

```sh
$ NGX_WASM_CARGO=0 NGX_BUILD_CC_OPT='-I/.../include' NGX_BUILD_LD_OPT='/.../lib/libngx_wasm_rs.a' make
```

For a dynamic build, the linker flags are slightly different:

```sh
$ NGX_WASM_CARGO=0 NGX_BUILD_CC_OPT='-I/.../include' NGX_BUILD_LD_OPT='-L/.../lib -Wl,-rpath=/.../lib -lngx_wasm_rs' make
```

If you are installing the dynamic library to a system location
known to the system's dynamic linker, such as `/usr/lib` or
`/usr/local/lib`, it is not necessary to pass the `-Wl,-rpath` flag.

**Note:** The `cargo build --features` flag used when building
ngx-wasm-rs must match the ones expected by our build system:

* `--features wat` when building for use with V8
* default features when building for use with Wasmer

**Note:** (Wasmer only) Due to a limitation when linking multiple static
libraries written in Rust that were compiled with different compilers, you
typically cannot link an upstream static library of Wasmer with a static build
of ngx-wasm-rs built with your local Rust compiler; in this case you have to
combine the static Wasmer library with a dynamic ngx-wasm-rs library.

## Test suites

### Run integration tests

Test suites are written with the
[Test::Nginx](https://metacpan.org/pod/Test::Nginx::Socket) Perl module and
extensions built on top of it.

Once the Nginx binary is built with ngx_wasm_module at `work/buildroot/nginx`,
the integration test suite can be run with:

```sh
$ make test
```

Under the hood, this runs the `util/test.sh` script. This script is used to
properly configure Test::Nginx for each run, and compile the Rust crates used in
the test cases. It supports many options mostly inherited from Test::Nginx and
specified via environment variables.

The tests can be run concurrently with:

```sh
$ TEST_NGINX_RANDOMIZE=1 make test
```

To run a subset of the test suite with Valgrind (slow):

```sh
$ TEST_NGINX_USE_VALGRIND=1 make test
```

To run the whole test suite with Valgrind (very slow):

```sh
$ TEST_NGINX_USE_VALGRIND_ALL=1 make test
```

To run the test suite by restarting workers with a HUP signal:

```sh
$ TEST_NGINX_USE_HUP=1 make test
```

To run the test suite and see a coverage report locally (requires
[Gcov](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html)):

```sh
$ make coverage
```

See [util/test.sh](util/test.sh) and the
[Test::Nginx](https://metacpan.org/pod/Test::Nginx::Socket) documentation for
a complete list of these options.

[Back to TOC](#table-of-contents)

### Run building tests

The test suite at `t/10-build` can be used to test that compilation options take
effect or that they can combine with each other. It can be run with:

```sh
$ make test-build
```

It is equivalent to:

```sh
$ util/test.sh --no-test-nginx t/10-build
```

[Back to TOC](#table-of-contents)

### Run individual tests

A subset of the test cases can be run via shell globbing:

```sh
$ ./util/test.sh t/01-wasm/001-wasm_block.t
$ ./util/test.sh t/03-proxy_wasm
$ ./util/test.sh t/03-proxy_wasm/{001,002,003}-*.t
```

To run a single test within a test file, add a line with `--- ONLY` to that test
case:

```
=== TEST 1: test name
--- ONLY
--- main_config
--- config
    location /t {
        ...
    }
--- error_log eval
...
```

Then run the test file in isolation:

```sh
$ ./util/test.sh t/02-http/001-wasm_call_directive.t
t/02-http/001-wasm_call_directive.t .. # I found ONLY: maybe you're debugging?
...
```

The Nginx running directory can be investigated in `t/servroot`, including error
logs.

[Back to TOC](#table-of-contents)

### Debug a test case

To reproduce a test case with an attached debugging session (e.g. with
[GDB](https://www.gnu.org/software/gdb/)), first isolate the test case and run
it:

```sh
# First edit the test and add --- ONLY block (see "Run individual tests")
$ ./util/test.sh t/02-http/001-wasm_call_directive.t
```

The above run produced the right `nginx.conf` configuration within `t/servroot`,
which is the default prefix directory for the binary at `work/buildroot/nginx`.

Then, start the debugger with the current binary and desired options:

```sh
$ gdb -ex 'b ngx_wasm_symbol' -ex 'r -g "daemon off;"' work/buildroot/nginx
```

Here, `daemon off` is one way of ensuring that the master process does not fork
into a daemon, so that the debugging session remains uninterrupted.

[Back to TOC](#table-of-contents)

## CI

The CI environment is built with GitHub Actions. Two workflows, `ci` and
`ci-large` are running on push and daily schedules, respectively. Both workflows
contain individual jobs, each for a specific testing mode:

- `ci`
    - Tests - unit
    - Tests - unit (valgrind)
    - Tests - build
    - Clang analyzer
    - Lint

- `ci-large`
    - Tests - unit (large)

Unit tests refer to `t/*.t` test files, and each unit tests job is ran as many
times as the workflow's matrix specifies. Currently, the matrices specifies that
jobs run:

- Both integration and building test suites.
- For several Operating Systems (Ubuntu, macOS).
- For multiple compilers (GCC, Clang).
- For several Nginx and WebAssembly runtimes and versions.
- With and without the Nginx "HUP reload" mode (`SIGHUP`).
- With and without the Nginx debug mode (`--with-debug`).
- With Valgrind memory testing mode (HUP on/off).
- Linting and static analyzer jobs.

The `ci-large` workflow specifies larger matrices that take longer to run, and
thus only run periodically.

[Back to TOC](#table-of-contents)

### Running the CI locally

It is possible to run the entire CI environment locally (or a subset of jobs) on
the only condition of having [Docker](https://docs.docker.com/get-docker/) and
[Act](https://github.com/nektos/act) installed:

```sh
$ make act
```

This will build the necessary Docker images for the build environment (Note:
Ubuntu is the only environment currently supported) and run Act.

[Back to TOC](#table-of-contents)

## Sources

The ngx_wasm_module sources are organized in an effort to respect the following
principles:

1. Implementing a reusable *Nginx Wasm VM* (`ngx_wavm`).
    - Supporting multiple WebAssembly runtimes (`ngx_wrt`).
    - Allowing for linking Wasm modules to Nginx host interfaces
      (`ngx_wavm_host`).
2. Sharing code common to all Nginx's subsystems (e.g. common to both
   `ngx_http_*` & `ngx_stream_*`).
3. Providing low-level, feature-complete C utilities for Nginx-related tasks and
   routines.

These principles enable the `ngx_http_wasm_module` use-case which uses
`ngx_wavm` the following way:

- First, implementing the host interface (`ngx_wavm_host`) of a given SDK
  (reusing the aforementioned low-level Nginx utilities),
- Then, invoking instances (`.wasm` bytecode linked to their Nginx host
  interface) as deemed appropriate in desired Nginx event handlers.

The same principles leave room for other Nginx subsystems to use `ngx_wavm` as
deemed appropriate, with as many utilities as possible already available.

[Back to TOC](#table-of-contents)

### Code layout

Roughly, the codebase is divided in the following way, from lower-level
building blocks up to the HTTP subsystem module:

`src/common/` — Common sources. All subsystem-agnostic code should be located
under this tree, including code enabling said agnosticism, aka
`ngx_wasm_subsystem`.

`src/common/shm` — Shared memory sources. Subsystem-agnostic component
implementing key/value and queue stores atop Nginx shared memory slabs.

`src/common/lua` — Sources for the Lua bridge. Two components (LuaJIT FFI + Lua
scheduler) allow for bilateral interactions between ngx_wasm_module and
ngx_lua_module.

`src/common/debug` — A debug-only module for testing and coverage purposes.

`src/wasm/` — Wasm subsystem sources. This is a hereby-implemented, new
subsystem: `ngx_wasm`. Its purpose is to configure and manage one or many Nginx
Wasm VM(s), aka `ngx_wavm`.

`src/wasm/wrt/` — Nginx Wasm runtime, or `ngx_wrt`. An interface encapsulating
multiple "Wasm bytecode execution engines" (Wasmer, Wasmtime...).

`src/wasm/wasi/` — A WASI host implementation provided by ngx_wasm_module.

`src/wasm/vm/` — Nginx Wasm VM, or `ngx_wavm`. An Nginx-tailored Wasm VM,
encapsulating `ngx_wrt` and providing routines to load `.wasm` modules and
invoke Wasm instances (`ngx_wavm_instance`).

`src/wasm/ngx_wasm_ops` — Wasm subsystem operations. An abstract pipeline
engine allowing the mixing of multiple "WebAssembly operations" that can be
executed for a given request/connection/server. A "WebAssembly operation" can be
"run a proxy-wasm filter chain", or "invoke this particular Wasm function", or
"do something else"...

`src/wasm/ngx_wasm_core_module` — Wasm subsystem core module. Each Nginx
subsystem has a core module executing its own subsystem's modules, this is this
so-called core module for the Wasm subsystem. It is mainly used as a way to
configure a "global Nginx Wasm VM" used by other subsystems' modules. Since
Nginx subsystems allow for extension points, this may be an avenue for
extensibility later on, with the addition of `ngx_wasm_*` modules and Wasm event
handlers (e.g. `create_instance_handler(ngx_wavm_t *vm, ngx_wavm_instance_t
*inst)` allowing tracking of instance creation in any `ngx_wasm_*` module).

`src/wasm/ngx_wasm_core_host` — Core Nginx host interface, exported under
`"ngx_wasm_core"`. This host interface is subsystem-agnostic.

`src/http/` — HTTP subsystem sources. HTTP-only code under this tree is
combined with `ngx_wasm_core_module`'s global `ngx_wavm`, to implement the
resulting `ngx_http_wasm_module`.

`src/http/ngx_http_wasm_host` — HTTP Nginx host interface, exported under
`"ngx_http_wasm"`.

`src/http/proxy_wasm/` — proxy-wasm-sdk state machine, or `ngx_proxy_wasm`. An
abstract proxy-wasm state machine for a chain of filters. Its resume handlers
are invoked as deemed appropriate by this ABI's user: `ngx_wasm_ops`, which
encapsulates all Wasm operations, including proxy-wasm filter chains execution.

`src/http/ngx_http_wasm_module` — HTTP subsystem module. High-level,
request-scoped invocations of `ngx_wasm_ops`, such as "run a proxy-wasm
filter chain".

[Back to TOC](#table-of-contents)

### Code lexicon

#### Terms

- **Crate** — *"Rust compilation units"*, or in this context also the equivalent
  of "packages" for the Rust ecosystem. The compilation units are compiled to
  libraries targeting `.wasm`, loaded and executed by this module. See
  [crates.io](https://crates.io/).

- **Host** — *"Wasm host environment"*. In the WebAssembly context, this term
  refers to the host application WebAssembly is embedded into, in this case,
  Nginx. See
  [Embedding](https://webassembly.github.io/spec/core/appendix/embedding.html?highlight=embed).

- **Host Interface** — *"Wasm host interface"*. An interface of values and
  functions allowing manipulation of the *host environment*'s state. These
  interfaces are imported by loaded Wasm modules, see
  [Imports](https://webassembly.github.io/spec/core/syntax/modules.html#imports).

- **Subsystem** — *"Nginx subsystem"*, or protocol-agnostic Nginx modules
  implementing the core principles of a protocol in Nginx. See
  [ngx_http_core_module](https://nginx.org/en/docs/http/ngx_http_core_module.html),
  implementing the HTTP subsystem for an example.

[Back to TOC](#table-of-contents)

#### Symbols

| Symbol                     | Description                                            |
| --------------------------:| -------------------------------------------------------|
| `ngx_wrt_*`                | *"Nginx Wasm runtime"*: Wasm bytecode execution engine (Wasmer, Wasmtime...).
| `ngx_wavm_*`               | *"Nginx Wasm VM"*: Wasm instances operations for Nginx.
| `ngx_wavm_host_*`          | *"Nginx Wasm VM host interface"*: host-side (i.e. Nginx) code imported by Wasm modules.
| `ngx_*_host`               | Implementations of various Wasm host interfaces.
| `ngx_wasm_*`               | Wasm subsystem code (loads and configure ngx_wavm) and subsystem-agnostic helpers.
| `ngx_wasm_lua_*`           | Wasm <-> Lua bridge code. Lua VM scheduler + LuaJIT FFI.
| `ngx_stream_wasm_*`        | Stream subsystem code, executing ngx_wavm appropriately.
| `ngx_http_wasm_*`          | HTTP subsystem code, executing ngx_wavm appropriately.
| `ngx_proxy_wasm_*`         | Subsystem-agnostic proxy-wasm-sdk code.
| `ngx_http_proxy_wasm_*`    | HTTP subsystem proxy-wasm-sdk code.
| `ngx_stream_proxy_wasm_*`  | Stream subsystem proxy-wasm-sdk code.

[Back to TOC](#table-of-contents)

## Profiling

Instructions for generating ngx_wasm_module CPU
[flamegraphs](https://github.com/brendangregg/FlameGraph) with
[perf](https://perf.wiki.kernel.org/index.php/Main_Page).

[Back to TOC](#table-of-contents)

### Wasmtime

First, compile a local, unoptimized profiling build of Wasmtime:

```sh
$ export NGX_BUILD_WASMTIME_PROFILE=profile
$ export NGX_BUILD_WASMTIME_RUSTFLAGS='-g -C opt-level=0 -C debuginfo=1'
$ ./util/runtime.sh --build --runtime wasmtime --force
```

Then, compile ngx_wasm_module against Wasmtime and tuned for profiling with:

```sh
$ export NGX_BUILD_DEBUG=0
$ export NGX_BUILD_CC_OPT='-O0 -g'
$ export NGX_WASM_RUNTIME=wasmtime
$ make
```

Next, configure ngx_wasm_module with the desired workload. For a default
workload, we suggest reusing the benchmark test suite:

```sh
# Build the NOP benchmark filter:
$ export TEST_NGINX_CARGO_PROFILE=
$ export TEST_NGINX_CARGO_RUSTFLAGS='-g -C opt-level=0 -C debuginfo=1'
# Generate t/servroot and t/servroot/conf/nginx.conf
$ ./util/test.sh t/11-bench/003-bench_proxy_wasm.t
```

Once the above test succeeds, review and edit `t/servroot/conf/nginx.conf`.
Make sure it is appropriate - for example there should only be one worker
process - and add the `perfmap` profiling flag:

```nginx
daemon           off; # optional
worker_processes 1;

wasm {
    wasmtime {
        flag profiler perfmap; # required
    }
}
```

Let's now start Nginx:

```sh
$ ./work/buildroot/nginx -p t/servroot
```

Once Nginx has started, you may start `perf` recording anytime:

```sh
$ perf record -p $(pgrep nginx) -g  # ensure -p is the worker process pid
# > perf.data
```

After recording, process the generated output with the [FlameGraph
tools](https://github.com/brendangregg/FlameGraph) to produce the flamegraph:

```sh
# perf.data > out.perf
$ perf script > out.perf
$ stackcollapse-perf.pl out.perf > out.folded
$ flamegraph.pl out.folded > out.svg
```

The resulting `out.svg` file is the produced flamegraph.

[Back to TOC](#table-of-contents)
