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

  $SCRIPT_NAME -R <runtime> [options]

EOF
}

show_help() {
    cat << EOF
Download or build a Wasm runtime.

EOF
    show_usage
    cat << EOF
Arguments:

  -R, --runtime <runtime>   Runtime to build (e.g. 'v8').

Options:

  --download                Prefer downloading a precompiled runtime.
                            This is the default.

  --build                   Prefer building a runtime from source.

  -A, --arch <arch>         Architecture in 'uname -m' format (e.g. 'x86_64').
                            Defaults to the system's own.

  -V, --runtime-ver <ver>   Runtime version to build (e.g. '10.5.18')
                            Inferred from .github/workflows/release.yml
                            if unspecified.

  -T, --target-dir <dir>    Where to put built files.
                            Defaults to $DIR_WORK/<runtime>-<ver>

  -f, --force, --clean      Force a clean build.

  -h, --help                Print this message and exit.

EOF
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
        -V|--runtime-ver|--runtime-version) # --runtime-version: deprecated
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
            echo "$SCRIPT_NAME version $(git rev-parse HEAD 2> /dev/null)"
            exit 0
            ;;
    esac
    shift
done

if [[ -z "$RUNTIME" ]]; then
    show_usage
    fatal "Bad usage. Consult --help"
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
    notice "deleting $TARGET_DIR..."
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
    notice "$MODE failed -- deleting $TARGET_DIR..."
    rm -rfv "$TARGET_DIR"
    exit 1
fi
