"""Setup Crates repostories """

load("@ngx_rust_crate_index//:defs.bzl", ngx_rust_crate_repositories = "crate_repositories")
load("@ngx_wasm_rs_crate_index//:defs.bzl", ngx_wasm_rs_crate_repositories = "crate_repositories")

def ngx_wasm_module_crates():
    ngx_rust_crate_repositories()
    ngx_wasm_rs_crate_repositories()
