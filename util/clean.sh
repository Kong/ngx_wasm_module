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
set +e

if [[ -f "$DIR_PATCHED_ROOT/Makefile" ]]; then
    pushd $DIR_PATCHED_ROOT
        make clean
        rm -f Makefile
    popd
fi

if [[ "$1" == "--all" || "$1" == "--more" ]]; then
    rm -rf t/servroot* \
        $DIR_SRC_ROOT \
        $DIR_PATCHED_ROOT \
        $DIR_BUILDROOT \
        $DIR_PREFIX \
        $DIR_DIST_OUT \
        $DIR_DIST_WORK \
    remove_luarocks
    cargo clean --manifest-path lib/Cargo.toml
    cargo clean --manifest-path t/lib/Cargo.toml
fi

if [[ "$1" == "--all" ]]; then
    rm -rf $DIR_WORK
fi

# vim: ft=sh ts=4 sts=4 sw=4:
