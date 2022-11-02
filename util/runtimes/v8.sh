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

    if [[ "$clean" = "clean" ]]; then
        rm -f "$tarball"
        rm -rfv "$target"
    fi

    if [[ ! -d "$tarball" ]]; then

        if [[ ! -e "$DIR_BIN/fetch" ]]; then
            local hostarch=$(uname -m)
            case $hostarch in
                x86_64)  hostarch='amd64';;
                aarch64) hostarch='arm64';;
            esac

            download $DIR_BIN/fetch \
                "https://github.com/gruntwork-io/fetch/releases/download/v0.4.5/fetch_${os}_${hostarch}"

            chmod +x $DIR_BIN/fetch
        fi

        local filename="ngx_wasm_runtime-v8-$version-$os-$arch.tar.gz"

        mkdir -p "$DIR_DOWNLOAD"

        # This requires a Personal Access Token in
        # the $GITHUB_OAUTH_TOKEN environment variable:
        $DIR_BIN/fetch \
            --repo "$URL_KONG_WASM_RUNTIMES" \
            --tag latest \
            --release-asset "$filename" \
            "$DIR_DOWNLOAD"

        mv "$DIR_DOWNLOAD/$filename" "$tarball"

        # TODO: replace all of the above with...
        #
        # download $tarball \
        #   "https://github.com/kong/ngx_wasm_runtimes/releases/download/latest/ngx_wasm_runtime-v8-${version}-${os}-${arch}.tar.gz"
        #
        # ...once ngx_wasm_runtimes is open-sourced
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

build_cwabt() {
    local target="$1"
    local clean="$2"

    notice "building lib/cwabt..."

    cd $NGX_WASM_DIR/lib/cwabt

    if [ "$clean" = "clean" ]; then
        make clean
    fi

    make
    make install TARGET="$target"
}

check_libwee8_build_dependencies() {
    python3 --help >/dev/null 2>/dev/null || {
        fatal "python3 is required in your path."
    }

    git --help >/dev/null 2>/dev/null || {
        fatal "git is required in your path."
    }

    xz --help >/dev/null 2>/dev/null || {
        fatal "xz (from xz-utils) is required in your path."
    }

    pkg-config --help >/dev/null 2>/dev/null || {
        fatal "pkg-config is required in your path."
    }

    curl --help >/dev/null 2>/dev/null || {
        fatal "curl is required in your path."
    }

    ninja --help >/dev/null 2>/dev/null
    [ "$?" = 1 ] || {
        fatal "ninja (from ninja-build) required in your path."
    }
}

build_libwee8() {
    local v8_ver="$1"
    local target="$2"
    local arch="$3"
    local clean="$4"

    case "$arch" in
        x86_64) arch="x64" ;;
    esac

    notice "building libwee8..."

    check_libwee8_build_dependencies || exit 1

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
        "is_clang=$is_clang"
        "use_sysroot=false"
        "is_debug=false"
        "use_custom_libcxx=false"
        "use_dbus=false"
        "use_glib=false"
        "v8_enable_webassembly=true"
        "v8_monolithic=true"
        "symbol_level=0"
        "target_cpu=\"$arch\""
        "v8_target_cpu=\"$arch\""
        "v8_use_external_startup_data=false"
        "treat_warnings_as_errors=false"
    )
    buildtools/*/gn gen out.gn/"$build_mode" --args="${args[*]}"

    notice "building V8..."
    ninja -C out.gn/"$build_mode" wee8

    ### install to target

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

    # On macOS builds, use the system-provided clang to avoid header issues
    if [[ "$(uname -s)" = "Darwin" ]]; then
        v8_cxx="clang"
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

    build_cwabt "$target" "$clean"
    build_libwee8 "$v8_ver" "$target" "$arch" "$clean"
    build_v8bridge "$target" "$clean"
}

###############################################################################

# TODO: change argument order to
# "$mode" "$target" "$v8_ver" "$arch" "$clean"
# once clients stop using v8.sh directly.
target="${1:-$DIR_WORK}"
v8_ver="${2:-$(get_variable_from_release_yml V8_VER)}"
arch="$3"
mode="$4"
clean="$5"

if [ "$mode" = "download" ]; then
    download_v8 "$target" "$v8_ver" "$arch" "$clean"
else
    build_v8 "$target" "$v8_ver" "$arch" "$clean"
fi
