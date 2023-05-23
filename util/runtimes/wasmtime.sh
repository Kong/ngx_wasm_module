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
NGX_BUILD_WASMTIME_RUSTFLAGS=${NGX_BUILD_WASMTIME_RUSTFLAGS:-$(get_variable_from_makefile NGX_BUILD_WASMTIME_RUSTFLAGS)}
NGX_BUILD_WASMTIME_PROFILE=${NGX_BUILD_WASMTIME_PROFILE:-$(get_variable_from_makefile NGX_BUILD_WASMTIME_PROFILE)}

download_wasmtime() {
    local target="$1"
    local version="$2"
    local arch="$3"
    local clean="$4"

    case $OSTYPE in
        linux*)  os="linux";;
        darwin*) os="macos";;
        *)       os=$OSTYPE;;
    esac

    tarball="$DIR_DOWNLOAD/wasmtime-$version.tar.xz"

    if [[ "$clean" = "clean" ]]; then
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

check_wasmtime_build_dependencies() {
    git --help >/dev/null 2>/dev/null || {
        fatal "git is required in your path."
    }

    cargo --help >/dev/null 2>/dev/null || {
        fatal "cargo is required in your path."
    }
}

build_wasmtime() {
    local target="$1"
    local version="$2"
    local arch="$3"
    local clean="$4"
    local profile="$NGX_BUILD_WASMTIME_PROFILE"

    notice "building Wasmtime..."
    check_wasmtime_build_dependencies || exit 1

    mkdir -p "$DIR_LIBWASMTIME"
    pushd "$DIR_LIBWASMTIME"

        ### fetch

        mkdir -p repos
        pushd repos
            if [ ! -d wasmtime ]; then
                notice "fetching Wasmtime repository..."
                git clone https://github.com/bytecodealliance/wasmtime.git

            else
                notice "synchronizing Wasmtime repository..."
                pushd wasmtime
                    git fetch
                popd
            fi

            pushd wasmtime
                git reset --hard "v$version"
                git submodule update --init
            popd
        popd

        ### build

        pushd repos/wasmtime
            local cargo_target_dir
            local args=()

            if [ "$profile" = debug -o "$profile" = profile ]; then
                cargo_target_dir="target/debug"

            else
                args+="--release"
                cargo_target_dir="target/release"
            fi

            if [ "$clean" = "clean" -a -d $cargo_target_dir ]; then
                rm -rf $cargo_target_dir
            fi

            RUSTFLAGS="$NGX_BUILD_WASMTIME_RUSTFLAGS" \
            eval cargo build \
                    "${args[@]}" \
                    --manifest-path crates/c-api/Cargo.toml

            ### install

            mkdir -p "$target/lib"
            cp $cargo_target_dir/libwasmtime.{a,so} "$target/lib"

            mkdir -p "$target/include"
            cp -R crates/c-api/include/* "$target/include"
            cp -R crates/c-api/wasm-c-api/include/* "$target/include"
        popd
    popd
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

if [ "$mode" = "download" ]; then
    download_wasmtime "$target" "$version" "$arch" "$clean"
else
    build_wasmtime "$target" "$version" "$arch" "$clean"
fi
