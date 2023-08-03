workspace(name = "ngx_wasm_module")

load("//build:repos.bzl", "ngx_wasm_module_repositories")

ngx_wasm_module_repositories()

load("//build:deps.bzl", "ngx_wasm_module_dependencies")

ngx_wasm_module_dependencies(cargo_home_isolated = False)  # use system `$CARGO_HOME` to speed up builds

load("//build:crates.bzl", "ngx_wasm_module_crates")

ngx_wasm_module_crates()
