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
    echo "usage: $0 -R <runtime> [-V <version>] [-T <target_dir>] [--clean]"
    echo
}

show_help() {
    echo "Download or build a Wasm runtime."
    echo
    show_usage
    echo "   --download"
    echo "      Prefer downloading a precompiled runtime."
    echo "      This is the default."
    echo
    echo "   --build"
    echo "      Prefer building a runtime from source."
    echo
    echo "   -R|--runtime <runtime>"
    echo "      Runtime to build (e.g. 'v8')"
    echo
    echo "   -V|--runtime-version <version>"
    echo "      Runtime version to build (e.g. '10.5.18')"
    echo "      Defaults to infer from .github/workflows/release.yml"
    echo
    echo "   -A|--arch <arch>"
    echo "      Architecture in 'uname -m' format (e.g. 'x86_64')"
    echo "      Defaults to the system's own"
    echo
    echo "   -T|--target-dir <target_dir>"
    echo "      Where to put built files"
    echo "      Defaults to $DIR_WORK/<runtime>-<version>"
    echo
    echo "   --clean|--force|-f"
    echo "      Force a clean build"
    echo
}

check_target() {
    local target="$1"

    if [[ $(ls "$target/lib" 2>/dev/null | wc -l) -gt 0 ]]; then
        notice "build already exists in $target - using it"
        return 0
    fi

    return 1
}

###############################################################################

MODE="download"
ARCH="$(uname -m)"

while [[ "$1" ]]; do
    case "$1" in
        -R|--runtime)
            shift
            RUNTIME="$1"
            ;;
        -V|--runtime-version)
            shift
            RUNTIME_VERSION="$1"
            ;;
        -A|--arch)
            shift
            ARCH="$1"
            ;;
        -T|--target-dir)
            shift
            TARGET_DIR="$1"
            ;;
        --download)
            MODE="download"
            ;;
        --build)
            MODE="build"
            ;;
        --clean|--force|-f)
            CLEAN="clean"
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        -v|--version)
            echo "$0 version $(git rev-parse HEAD 2> /dev/null)"
            echo "Run $0 --help for usage."
            echo
            exit 0
            ;;
    esac
    shift
done

if [[ -z "$RUNTIME" ]]; then
    show_usage
    echo "Run $0 --help for details."
    echo
    exit 1
fi

if [[ -z "$RUNTIME_VERSION" ]]; then
    RUNTIME_VERSION="$(get_default_runtime_version "$RUNTIME")"
fi

if [[ -z "$TARGET_DIR" ]]; then
    TARGET_DIR="$(get_default_runtime_dir "$RUNTIME" "$RUNTIME_VERSION")"
else
    TARGET_DIR=$(abs_path $TARGET_DIR)
fi

if [[ "$CLEAN" = "clean" ]]; then
    echo "Deleting $TARGET_DIR..."
    rm -rfv "$TARGET_DIR"
fi

if [[ -n "$CI" ]]; then
    echo "NGX_WASM_RUNTIME=$RUNTIME" >> $GITHUB_ENV
    echo "NGX_WASM_RUNTIME_INC=$TARGET_DIR/include" >> $GITHUB_ENV
    echo "NGX_WASM_RUNTIME_LIB=$TARGET_DIR/lib" >> $GITHUB_ENV
fi

if check_target "$TARGET_DIR"; then
    exit 0
fi

BUILD_SCRIPT=$NGX_WASM_DIR/util/runtimes/$RUNTIME.sh

# TODO: change argument order to
# "$MODE" "$TARGET_DIR" "$RUNTIME_VERSION" "$ARCH" "$CLEAN"
# once clients stop using v8.sh directly.
if "$BUILD_SCRIPT" "$TARGET_DIR" "$RUNTIME_VERSION" "$ARCH" "$MODE" "$CLEAN"; then
    exit 0
else
    echo "$MODE failed -- deleting $TARGET_DIR..."
    rm -rfv "$TARGET_DIR"
    exit 1
fi
