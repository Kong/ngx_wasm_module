# Developer documentation

The building process for ngx_wasm_module's is inherent to that of Nginx and
therefore relies on `make`. Additionally, a WebAssembly runtime should be
compiled and available in the system as a shared library.

The below instructions will guide you through the development environment and
idiomatic workflow for ngx_wasm_module. It has not been tested in many
environments yet and may still need refinements; reports are very much welcome.

## Table of Contents

- [Requirements](#requirements)
    - [WebAssembly runtime](#webassembly-runtime)
    - [Nginx](#nginx)
- [Setup the environment](#setup-the-environment)
- [Build from source](#build-from-source)
    - [Build options](#build-options)
- [Workflow](#workflow)
    - [Run tests](#run-tests)
    - [Run individual tests](#run-individual-tests)
    - [Debug a test case](#debug-a-test-case)
- [Makefile](#makefile)
- [CI](#ci)
    - [Running the CI locally](#running-the-ci-locally)

## Requirements

Most of the development of this project currently relies on testing WebAssembly
modules produced from Rust crates. Hence, while not technically a requirement to
compile ngx_wasm_module or to produce Wasm bytecode, having Rust installed on
the system will quickly become necessary for development:

- [rustup.rs](https://rustup.rs/) is the easiest way to install Rust.
- WebAssembly runtime (see [Installation Requirements](README.md#requirements)).

[Back to TOC](#table-of-contents)

### Nginx

To build Nginx from source requires the following dependencies be installed on
the system; here are the packages for various platforms:

On Ubuntu:

```
$ apt-get install build-essential libpcre3-dev zlib1g-dev libgd-dev perl curl
```

> See [ubuntu-wasmx-dev/Dockerfile](util/ubuntu-wasmx-dev/Dockerfile) for a
  complete list of Ubuntu development & CI dependencies.

On Fedora/RedHat (not tested):

```
$ yum install gcc pcre-devel zlib perl curl
```

On Arch Linux:

```
$ pacman -S gcc lib32-pcre zlib perl curl
```

On macOS (not tested):

```
$ xcode-select --install
$ brew install pcre zlib-devel libgd perl curl
```

[Back to TOC](#table-of-contents)

## Setup the environment

Setup a `work/` directory which will bundle all of the extra building and
testing dependencies:

```
$ make setup
```

This makes the building process of ngx_wasm_module entirely self-contained. The
build environment may be destroyed at anytime with:

```
$ make cleanup
```

[Back to TOC](#table-of-contents)

## Build from source

If you have installed Wasmtime, build ngx_wasm_module with:

```
$ make
```

This should download the default Nginx version, compile and link it and produce
a static binary at `./work/buildroot/nginx`.

If you want to link ngx_wasm_module to Wasmer instead, use:

```
$ NGX_WASM_RUNTIME=wasmer make
```

You may specify a different path for the WebAssembly runtimes headers and shared
libraries, like so:

```
$ NGX_WASM_RUNTIME_INC=/path/to/headers/ NGX_WASM_RUNTIME_LIB=/path/to/libs/ make
```

[Back to TOC](#table-of-contents)

### Build options

The build system offered by the Makefile offers many options, for example, build
with Clang, Nginx 1.19.9, and without debug mode:

```
$ CC=clang NGX=1.19.9 NGX_BUILD_DEBUG=0 make
```

The build system will download the Nginx release specified in `NGX` and build
ngx_wasm_module against it; or if `NGX` points to cloned Nginx sources, the
build system will build ngx_wasm_module against these sources.

Build with additional compiler options:

```
$ NGX_BUILD_CC_OPT="-g -O3" make
```

Build with [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html):

```
$ CC=clang NGX_BUILD_FSANITIZE=address make
```

Build with [Clang Static Analyzer](https://clang-analyzer.llvm.org/):

```
$ CC=clang NGX_BUILD_CLANG_ANALYZER=1 make
```

Build with [Gcov](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html):

```
$ CC=gcc NGX_BUILD_GCOV=1 make
```

Build with the Nginx [no-pool patch](https://github.com/openresty/no-pool-nginx)
for memory analysis with Valgrind:

```
$ NGX_BUILD_NOPOOL=1 make
```

All build options may be mixed and matched together. Consecutive builds using
the same options will be incremental thanks to `make`. Changing one or several
options will automatically trigger a fresh build.

[Back to TOC](#table-of-contents)

## Workflow

### Run tests

Test suites are written with the
[Test::Nginx](https://metacpan.org/pod/Test::Nginx::Socket) Perl module and
extensions built on top of it.

Once the module built, the entire test suite can be run with:

```
$ make test
```

It is equivalent to:

```
$ util/test.sh t/0*
```

The `util/test.sh` script is used to properly configure Test::Nginx for each
run, and compile the Rust crates used in the test cases. It supports many
options mostly inherited from Test::Nginx, for example, to run the test suite
by restarting workers with a HUP signal, use:

```
$ TEST_NGINX_USE_HUP=1 ./util/test.sh t/0*
```

The tests can be run concurrently with:

```
$ ./util/test.sh t/0* -j # -j detects number of cores
```

See [util/test.sh](util/test.sh) and the
[Test::Nginx](https://metacpan.org/pod/Test::Nginx::Socket) documentation for a
list of these options.

[Back to TOC](#table-of-contents)

### Run individual tests

To run a single test suite, point the test script to a set of one or more `.t`
files or directories containing test files, for example:

```
$ ./util/test.sh t/02-http
$ ./util/test.sh t/02-http/001-wasm_call_directive.t
```

To run a single test within a test file, add a line with `--- ONLY` to that test
case, like so:

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

And run the test file as previously described, like so:

```
$ ./util/test.sh t/02-http/001-wasm_call_directive.t
t/02-http/001-wasm_call_directive.t .. # I found ONLY: maybe you're debugging?
...
```

[Back to TOC](#table-of-contents)

### Debug a test case

To reproduce a test case with an attached debugging session (e.g. with
[GDB](https://www.gnu.org/software/gdb/)), first isolate the test case and run
it:

```
# First edit and add --- ONLY block (see "Run individual tests")
$ ./util/test.sh t/02-http/001-wasm_call_directive.t
```

The above run produced the right `nginx.conf` configuration within `t/servroot`,
which is the default prefix directory for the binary at `work/buildroot/nginx`.

Then, start the debugger with the current binary and desired options:

```
$ gdb -ex 'b ngx_wasm_symbol' -ex 'r -g "daemon off;"' work/buildroot/nginx
```

Here, `daemon off` is one way of ensuring that the master process does not fork
into a daemon, so that the debugging session remains uninterrupted.

[Back to TOC](#table-of-contents)

### Build options test suite

The test suite at `t/10-build` can be used to test that compilation options take
effect or that they can combine with each other. It can be run with:

```
$ make test-build
```

It is equivalent to:

```
$ util/test.sh --no-test-nginx t/10-build
```

[Back to TOC](#table-of-contents)

## Makefile


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
| `cleanup`          | Destroy the build environment

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

- For several Operating Systems (Ubuntu, macOS)
- For multiple compilers (GCC, Clang)
- For several Nginx and WebAssembly runtimes versions (Note: for the moment,
  Wasmtime is the only supported runtime on the CI environment).
- With and without the Nginx "HUP reload" mode (`SIGHUP`).
- With and without the Nginx debug mode (`--with-debug`).

The `ci-large` workflow specifies larger matrices that take longer to run, and
thus only do so during daily scheduled jobs.

[Back to TOC](#table-of-contents)

### Running the CI locally

It is possible to run the entire CI environment locally (or a subset of jobs) on
the only condition of having [Docker](https://docs.docker.com/get-docker/) and
[Act](https://github.com/nektos/act) installed:

```
$ make act
```

This will build the necessary Docker images for the build environment (Note:
Ubuntu is the only environment currently supported) and run Act.

[Back to TOC](#table-of-contents)
