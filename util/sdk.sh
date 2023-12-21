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

show_usage() {
    cat << EOF
Usage:

  $SCRIPT_NAME -S <sdk> [options]

EOF
}

show_help() {
    cat << EOF
Download or build a Proxy-Wasm SDK.

EOF
    show_usage
    cat << EOF
Arguments:

  -S, --sdk <sdk>           Proxy-Wasm SDK to build (e.g. 'go').

Options:

  --download                Download a Proxy-Wasm SDK source code.
                            This is the default.

  --build                   Build a Proxy-Wasm SDK and example filters.

  --install                 Copy all compiled Proxy-Wasm SDK example filters
                            to work/lib/wasm for test suites.

  -V, --sdk-ver <ver>       Proxy-Wasm SDK version to build (e.g. '0.2.1')

  -f, --force, --clean      Force a clean build with --build,
                            or a clean download with --download.

  -h, --help                Print this message and exit.

EOF
}

###############################################################################

MODE="download"

while [[ "$1" ]]; do
    case "$1" in
        -S|--sdk)
            shift
            PROXY_WASM_SDK="$1"
            ;;
        -V|--sdk-ver)
            shift
            PROXY_WASM_SDK_VERSION="$1"
            ;;
        --download)
            MODE="download"
            ;;
        --build)
            MODE="build"
            ;;
        --install)
            MODE="install"
            ;;
        --clean|--force|-f)
            CLEAN="clean"
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        -v|--version)
            echo "$SCRIPT_NAME version $(git rev-parse HEAD 2> /dev/null)"
            exit 0
            ;;
    esac
    shift || true
done

if [[ -z "$PROXY_WASM_SDK" ]]; then
    show_usage
    fatal "Bad usage. Consult --help"
fi

if [[ -z "$PROXY_WASM_SDK_VERSION" ]]; then
    PROXY_WASM_SDK_VERSION="$(get_default_proxy_wasm_sdk_version "$PROXY_WASM_SDK")"
fi

BUILD_SCRIPT=$NGX_WASM_DIR/util/sdks/$PROXY_WASM_SDK.sh

if "$BUILD_SCRIPT" "$MODE" "$PROXY_WASM_SDK_VERSION" "$CLEAN"; then
    exit 0

else
    notice "$MODE failed"
    exit 1
fi
