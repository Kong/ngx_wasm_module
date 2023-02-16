#!/usr/bin/env bash
set -e

SCRIPT_NAME=$(basename $0)
NGX_WASM_DIR=${NGX_WASM_DIR:-"$(
    cd $(dirname $(dirname $(dirname ${0})))
    pwd -P
)"}
if [[ ! -f "${NGX_WASM_DIR}/util/_lib.sh" ]]; then
    echo "Unable to source util/_lib.sh" >&2
    exit 1
fi

source $NGX_WASM_DIR/util/_lib.sh

###############################################################################

download_wasmtime() {
    local target="$1"
    local version="$2"
    local arch="$3"
    local clean="$4"

    case $OSTYPE in
        linux*)  os="linux" ;;
        darwin*) os="macos" ;;
        *)       os=$OSTYPE
    esac

    tarball="$DIR_DOWNLOAD/wasmtime-$version.tar.xz"

    if [[ "$clean" = "clean" ]]; then
        rm -f "$tarball"
        rm -rfv "$target"
    fi

    download $tarball \
        "https://github.com/bytecodealliance/wasmtime/releases/download/v$version/wasmtime-v$version-$arch-$os-c-api.tar.xz"

    if [[ ! -d "$target" ]]; then
        local parent="$(dirname "$target")"
        mkdir -p "$parent"
        pushd "$parent"
            tar -xf "$tarball"
            mv wasmtime-v$version-$arch-$os-c-api "$target"
        popd
    fi
}

###############################################################################

# TODO: change argument order to
# "$mode" "$target" "$version" "$arch" "$clean"
# once clients stop using v8.sh directly.
target="${1:-$DIR_WORK}"
version="${2:-$(get_variable_from_makefile WASMTIME)}"
arch="${3:-$(uname -m)}"
mode="$4"
clean="$5"

download_wasmtime "$target" "$version" "$arch" "$clean"
