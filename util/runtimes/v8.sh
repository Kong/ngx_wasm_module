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

download_v8() {
    local target="$1"
    local version="$2"
    local arch="$3"
    local clean="$4"

    local os=$(uname -s | tr '[:upper:]' '[:lower:]')
    local tarball="$DIR_DOWNLOAD/v8-$version.tar.gz"

    case "$arch" in
        aarch64) arch="arm64" ;;
    esac

    if [[ "$clean" = "clean" ]]; then
        rm -rfv "$target"
    fi

    if [[ ! -d "$tarball" ]]; then
        download $tarball \
          "$URL_KONG_WASM_RUNTIMES/releases/download/latest/ngx_wasm_runtime-v8-${version}-${os}-${arch}.tar.gz"
    fi

    if [[ ! -d "$target" ]]; then
        local parent="$(dirname "$target")"
        mkdir -p "$parent"
        pushd "$parent"
            tar -xf "$tarball"
            mv v8-$version-$os-$arch "$target"
        popd
    fi
}

###############################################################################

check_libwee8_build_dependencies() {
    git --help >/dev/null 2>/dev/null || {
        fatal "git is required in your path."
    }

    curl --help >/dev/null 2>/dev/null || {
        fatal "curl is required in your path."
    }

    python3 --help >/dev/null 2>/dev/null || {
        fatal "python3 is required in your path."
    }

    xz --help >/dev/null 2>/dev/null || {
        fatal "xz (from xz-utils) is required in your path."
    }

    pkg-config --help >/dev/null 2>/dev/null || {
        fatal "pkg-config is required in your path."
    }

    ninja --help >/dev/null 2>/dev/null
    [ "$?" = 1 ] || {
        fatal "ninja (from ninja-build) required in your path."
    }
}

build_libwee8() {
    local target="$1"
    local clean="$2"
    local v8_ver="$3"
    local arch="$4"

    notice "building libwee8..."
    check_libwee8_build_dependencies || exit 1

    case "$arch" in
        x86_64) arch="x64" ;;
        aarch64) arch="arm64" ;;
    esac

    mkdir -p "$DIR_LIBWEE8"
    cd "$DIR_LIBWEE8"

    mkdir -p tools

    if [ ! -e tools/depot_tools ]; then
        cd tools
        git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
        cd ..
    fi

    export PATH="$DIR_LIBWEE8/tools/depot_tools:$PATH"

    # Running as root causes issues when the V8 build process uses tar to
    # extract archives: tar sets file permissions differently when run as root.
    {
        local tar="$DIR_LIBWEE8/tools/depot_tools/tar"
        rm -f "$tar"
        echo "#!/bin/sh" > "$tar"
        echo "exec $(which tar)" '"$@"' "--no-same-owner --no-same-permissions" >> "$tar"
        chmod +x "$tar"
    }

    # Ensure plain 'python' is Python 3
    ( python --version | grep -q "Python 3" ) || {
        local python="$DIR_LIBWEE8/tools/depot_tools/python"
        rm -f "$python"
        echo "#!/bin/sh" > "$tar"
        echo "exec $(which python3)" '"$@"' >> "$python"
        chmod +x "$python"
    }

    ### fetch

    mkdir -p repos

    if [ ! -e repos/v8 ]; then
        cd repos
        notice "fetching V8 repository..."
        fetch v8
        cd ..
    fi

    cd repos/v8

    git checkout .
    git checkout "$v8_ver"

    notice "synchronizing V8 repository..."
    gclient sync

    ### build

    local build_mode="$arch.release"

    if [ "$clean" = "clean" ]; then
        rm -rf out.gn/"$build_mode"
        rm -rf gn
    fi

    buildtools/*/gn --help &>/dev/null || {
        if [ ! -e gn ]; then
            (
                export CXX=g++
                notice "building gn for the target system..."
                git clone https://gn.googlesource.com/gn
                cd gn
                python build/gen.py --allow-warning
                ninja -C out
                cd ..
                cp gn/out/gn buildtools/*/gn
            )
        fi
    }

    export PKG_CONFIG_PATH=/usr/lib/$(gcc -print-multiarch 2>/dev/null)/pkgconfig:/usr/lib/pkgconfig:$PKG_CONFIG_PATH

    local is_clang=false
    if [ "$CC" = "clang" ]; then
        is_clang=true
    fi

    notice "generating V8 build files for $arch..."
    local args=(
        "target_cpu=\"$arch\""
        "is_clang=$is_clang"
        "is_debug=false"
        "symbol_level=0"
        "use_glib=false"
        "use_dbus=false"
        "use_sysroot=false"
        "use_custom_libcxx=false"
        "v8_target_cpu=\"$arch\""
        "v8_monolithic=true"
        "v8_enable_webassembly=true"
        "v8_use_external_startup_data=false"
        "treat_warnings_as_errors=false"
        "fatal_linker_warnings=false"
    )
    buildtools/*/gn gen out.gn/"$build_mode" --args="${args[*]}"

    notice "building V8..."
    ninja -C out.gn/"$build_mode" wee8

    ### install

    mkdir -p "$target/lib"
    cp out.gn/"$build_mode"/obj/libwee8.a "$target/lib"

    mkdir -p "$target/include"
    cp ./third_party/wasm-api/wasm.h "$target/include"
}

build_v8bridge() {
    local target="$1"
    local clean="$2"

    notice "building v8bridge..."

    if [ "$clean" = "clean" ]; then
        make -C "$NGX_WASM_DIR/lib/v8bridge" clean
    fi

    # Use the same V8 clang toolchain to build v8bridge - C++ ABI compatibility
    local v8_cxx="$DIR_LIBWEE8/repos/v8/third_party/llvm-build/Release+Asserts/bin/clang"

    if [[ "$(uname -s)" = "Darwin" ]]; then
        # On macOS builds, use the system-provided clang to avoid header issues
        v8_cxx="clang"

    elif [[ "$(uname -m)" = "aarch64" ]]; then
        # V8 clang is built for x86_64; if running on arm64 defaults to gcc
        v8_cxx="gcc"
    fi

    make -C "$NGX_WASM_DIR/lib/v8bridge" \
        CXX="$v8_cxx" \
        V8_INCDIR="$DIR_LIBWEE8/repos/v8/include" \
        build
    make -C "$NGX_WASM_DIR/lib/v8bridge" TARGET="$target" install
}

build_v8() {
    local target="$1"
    local v8_ver="$2"
    local arch="$3"
    local clean="$4"

    build_libwee8 "$target" "$clean" "$v8_ver" "$arch"
    build_v8bridge "$target" "$clean"
}

###############################################################################

# TODO: change argument order to
# "$mode" "$target" "$v8_ver" "$arch" "$clean"
# once clients stop using v8.sh directly.
target="${1:-$DIR_WORK}"
v8_ver="${2:-$(get_variable_from_makefile V8)}"
arch="$3"
mode="$4"
clean="$5"

if [ "$mode" = "download" ]; then
    download_v8 "$target" "$v8_ver" "$arch" "$clean"

elif [[ -n "$CI" && "$arch" = "aarch64" ]]; then
    # building ARM v8 over qemu on CI is expensive
    notice "running on aarch64 CI: forcing download mode"
    download_v8 "$target" "$v8_ver" "$arch" "$clean"

else
    build_v8 "$target" "$v8_ver" "$arch" "$clean"
fi
