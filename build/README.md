# Bazel project for ngx_wasm_module Rust crates

Note that you only need to import this rule when using either `wasmer` or `v8` runtime, which
requires the `ngx_wasm_rs` library. This rule is not required for `wasmtime` runtime.

## Integration

To use in other Bazel projects, add the following to your WORKSPACE file:

```python

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "ngx_wasm_module",
    branch = "some-tag",
    remote = "https://github.com/Kong/ngx_wasm_module",
)

load("@ngx_wasm_module//build:repos.bzl", "ngx_wasm_module_repositories")

ngx_wasm_module_repositories()

load("@ngx_wasm_module//build:deps.bzl", "ngx_wasm_module_dependencies")

ngx_wasm_module_dependencies(cargo_home_isolated = False) # use system `$CARGO_HOME` to speed up builds

load("@ngx_wasm_module//build:crates.bzl", "ngx_wasm_module_crates")

ngx_wasm_module_crates()

```

The following example shows how to build OpenResty with ngx_wasm_module with `wasmer` as runtime and link dynamically
to the `ngx_wasm_rs` library:

```python
configure_make(
    name = "openresty",
    env = {
        "NGX_WASM_RUNTIME": "wasmer",
        "NGX_WASM_CARGO": "0",
    }
    configure_options = [
        "--with-cc-opt=\"-I$$EXT_BUILD_DEPS$$/ngx_wasm_rs/include\"",
        "--with-ld-opt=\"-L$$EXT_BUILD_DEPS$$/ngx_wasm_rs/lib\"",
    ],
    # ...
    deps = [
        "@ngx_wasm_module//:ngx_wasm_rs",
    ],
)
```

When building this library in Bazel, use the `-c opt` flag to ensure optimal performance. The default fastbuild mode produces a less performant binary.
