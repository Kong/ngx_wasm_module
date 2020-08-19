#!/usr/bin/env bash
set -e

SCRIPT_NAME=$(basename $0)
NGX_WASM_DIR=${NGX_WASM_DIR:-"$(
    cd $(dirname $(dirname ${0}))
    pwd -P
)"}
if [[ ! -f "${NGX_WASM_DIR}/util/_lib.sh" ]]; then
    echo "Unable to source util/_lib.sh" >&2
    exit 1
fi

source $NGX_WASM_DIR/util/_lib.sh

###############################################################################

pushd $DIR_SRCROOT
    make clean
popd

if [[ "$1" == "--all" ]]; then
    rm -rf $DIR_WORK $DIR_PREFIX
    cargo clean --manifest-path=t/lib/rust-http-tests/Cargo.toml
    cargo clean --manifest-path=lib/ngx-rust/Cargo.toml
fi

# vim: ft=sh st=4 sts=4 sw=4:
