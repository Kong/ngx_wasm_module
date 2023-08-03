"""Setup dependencies after repostories are downloaded."""

load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies", "rust_register_toolchains", "rust_repository_set")
load("@rules_rust//crate_universe:repositories.bzl", "crate_universe_dependencies")
load("@rules_rust//crate_universe:defs.bzl", "crates_repository")

def ngx_wasm_module_dependencies(cargo_home_isolated = True):
    """
    ngx_wasm_module_dependencies setup rust toolchain and cargo dependency repositories.

    Args:
        cargo_home_isolated (bool): cargo_home_isolated to False to reuse system CARGO_HOME
        for faster builds. cargo_home_isolated is default False and will use isolated
        Cargo home, which takes about 2 minutes to bootstrap.
    """
    rules_rust_dependencies()

    rust_register_toolchains(
        edition = "2018",
        extra_target_triples = ["aarch64-unknown-linux-gnu"],
    )

    rust_repository_set(
        name = "rust_linux_arm64_linux_tuple",
        edition = "2018",
        exec_triple = "x86_64-unknown-linux-gnu",
        extra_target_triples = ["aarch64-unknown-linux-gnu"],
        versions = ["stable"],
    )

    crate_universe_dependencies()

    crates_repository(
        name = "ngx_rust_crate_index",
        cargo_lockfile = "@ngx_wasm_module//:lib/ngx-rust/Cargo.lock",
        manifests = [
            "@ngx_wasm_module//:lib/ngx-rust/Cargo.toml",
        ],
        isolated = cargo_home_isolated,
    )

    crates_repository(
        name = "ngx_wasm_rs_crate_index",
        cargo_lockfile = "@ngx_wasm_module//:lib/ngx-wasm-rs/Cargo.lock",
        manifests = [
            "@ngx_wasm_module//:lib/ngx-wasm-rs/Cargo.toml",
        ],
        isolated = cargo_home_isolated,
    )

def normalize_name(n):
    return n.replace("-", "_")
