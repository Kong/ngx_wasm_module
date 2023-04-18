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
NGX_BUILD_WASMER_RUSTFLAGS=${NGX_BUILD_WASMER_RUSTFLAGS:-$(get_variable_from_makefile NGX_BUILD_WASMER_RUSTFLAGS)}

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

check_wasmer_build_dependencies() {
    git --help >/dev/null 2>/dev/null || {
        fatal "git is required in your path."
    }

    cargo --help >/dev/null 2>/dev/null || {
        fatal "cargo is required in your path."
    }

    # LLVM disabled: https://github.com/Kong/ngx_wasm_module/pull/265
    #llvm-config --version >/dev/null 2>/dev/null \
    #|| llvm-config-11 --version >/dev/null 2>/dev/null \
    #|| llvm-config-12 --version >/dev/null 2>/dev/null \
    #|| llvm-config-13 --version >/dev/null 2>/dev/null \
    #|| llvm-config-14 --version >/dev/null 2>/dev/null \
    #|| llvm-config-15 --version >/dev/null 2>/dev/null \
    #|| {
    #    fatal "llvm-config not in path, is LLVM installed?"
    #}
}

build_wasmer() {
    local target="$1"
    local version="$2"
    local arch="$3"
    local clean="$4"

    notice "building Wasmer..."
    check_wasmer_build_dependencies || exit 1

    mkdir -p "$DIR_LIBWASMER"
    pushd "$DIR_LIBWASMER"

        ### fetch

        mkdir -p repos
        pushd repos
            if [ ! -d wasmer ]; then
                notice "fetching Wasmer repository..."
                git clone https://github.com/wasmerio/wasmer.git

            else
                notice "synchronizing Wasmer repository..."
                pushd wasmer
                    git fetch
                popd
            fi

            pushd wasmer
                git reset --hard "v$version"
            popd
        popd

        ### build

        pushd repos/wasmer
            if [ "$clean" = "clean" -a -d target/release ]; then
                rm -rf target
            fi

            RUSTFLAGS=$NGX_BUILD_WASMER_RUSTFLAGS \
            ENABLE_LLVM=0 \
                make build-capi

            ### install

            mkdir -p "$target/lib"
            cp target/release/libwasmer*.{a,so} "$target/lib"

            mkdir -p "$target/include"
            cp -R lib/c-api/wasmer*.h "$target/include"
            cp -R lib/c-api/tests/wasm-c-api/include/*.h "$target/include"
        popd
    popd
}

###############################################################################

# TODO: change argument order to
# "$mode" "$target" "$version" "$arch" "$clean"
# once clients stop using v8.sh directly.
target="${1:-$DIR_WORK}"
version="${2:-$(get_variable_from_makefile WASMER)}"
arch="${3:-$(uname -m)}"
mode="$4"
clean="$5"

if [ "$mode" = "download" ]; then
    download_wasmer "$target" "$version" "$arch" "$clean"
else
    build_wasmer "$target" "$version" "$arch" "$clean"
fi
