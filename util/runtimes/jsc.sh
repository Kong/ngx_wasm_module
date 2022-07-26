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

if [ -z "$JSC_DIR" ]; then
    echo "Missing JSC_DIR"
    exit 1
fi

export CC=clang
export CXX=clang++

build_jscbridge() {
    local target="$1"

    notice "building jscbridge..."
    make -C "$NGX_WASM_DIR/lib/jscbridge" \
        JSC_INCDIR="$DIR_JSC_BUILD/JavaScriptCore/Headers" \
        build
    make -C "$NGX_WASM_DIR/lib/jscbridge" TARGET="$target" install
}

build_jsc() {
    local target="$1"

    pushd .
    mkdir -p "$DIR_JSC_BUILD"
    cd "$DIR_JSC_BUILD"
    cmake "$JSC_DIR" \
        -DPORT=JSCOnly \
        -DENABLE_STATIC_JSC=ON \
        -DUSE_THIN_ARCHIVES=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_FTL_JIT=ON \
        -DENABLE_JIT=ON \
        -DCMAKE_C_FLAGS="" \
        -DCMAKE_CXX_FLAGS="-std=c++20" \
        -G Ninja

    cmake --build . --config Release -- jsc
    popd

    mkdir -p "$target/lib"
    mkdir -p "$target/include"

    cp $DIR_JSC_BUILD/lib/*.a "$target/lib/"
    build_jscbridge "$target"
}

target="${1:-$DIR_WORK}"
build_jsc "$target"
