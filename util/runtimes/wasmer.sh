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

download_wasmer() {
    local target="$1"
    local version="$2"
    local arch="$3"
    local clean="$4"

    case $arch in
        x86_64)  arch='amd64';;
        aarch64) arch='arm64';;
    esac

    kernel=$(uname -s | tr '[:upper:]' '[:lower:]')
    tarball="$DIR_DOWNLOAD/wasmer-$version.tar.gz"

    if [[ "$clean" = "clean" ]]; then
        rm -f "$tarball"
        rm -rfv "$target"
    fi

    local url_version=$version
    local major_version="${version%%.*}"
    if [[ "$major_version" -ge 3 ]]; then
        url_version=v$version
    fi

    download $tarball \
        "https://github.com/wasmerio/wasmer/releases/download/$url_version/wasmer-$kernel-$arch.tar.gz"

    if [[ ! -d "$target" ]]; then
        mkdir -p "$target"
        tar --directory="$target" -xf "$tarball"
    fi
}

###############################################################################

# TODO: change argument order to
# "$mode" "$target" "$version" "$arch" "$clean"
# once clients stop using v8.sh directly.
target="${1:-$DIR_WORK}"
version="${2:-$(get_variable_from_release_yml WASMER_VER)}"
arch="${3:-$(uname -m)}"
mode="$4"
clean="$5"

download_wasmer "$target" "$version" "$arch" "$clean"
