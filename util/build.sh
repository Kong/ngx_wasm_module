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
    echo "Usage: $SCRIPT_NAME <NGX> [options]"
    echo
    echo "<NGX>             Version of NGINX to build (e.g. 1.17.9)"
    echo "                  or path to an NGINX source tree (e.g. /tmp/nginx)."
    echo
    echo "Options:"
    echo "  -f,--force      Force a complete build (no incremental build)."
    echo
    echo "  -h,--help       Print this message and exit."
}

###############################################################################

while (( "$#" )); do
    case $1 in
        -h|--help)
            show_usage
            exit
            ;;
        -f|--force)
            NGX_BUILD_FORCE=1
            shift
            ;;
        *)
            PARAMS="$PARAMS $1"
            shift
            ;;
    esac
done

eval set -- "$PARAMS"

NGX=$1

if [[ -z "$NGX" ]]; then
    invalid_usage
fi

if [[ -d "$NGX" ]]; then
    NGX_DIR=$NGX
    NGX_VER=$(awk 'match($0, /NGINX_VERSION\s+"(.*?)"/, m) {print m[1]}' $NGX_DIR/src/core/nginx.h)

else
    NGX_VER=$NGX

    if [[ ! "$NGX_VER" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        invalid_usage "version must be of form 'X.Y.Z' (e.g. 1.17.9)"
    fi

    download $DIR_DOWNLOAD/nginx-$NGX_VER.tar.gz \
             "https://nginx.org/download/nginx-$NGX_VER.tar.gz"

    if [[ ! -d "$DIR_DOWNLOAD/nginx-$NGINX_VER" ]]; then
        tar -xf $DIR_DOWNLOAD/nginx-$NGX_VER.tar.gz -C $DIR_DOWNLOAD
    fi

    NGX_DIR=$DIR_DOWNLOAD/nginx-$NGX_VER
fi

build_nginx $NGX_DIR $NGX_VER

# vim: ft=sh st=4 sts=4 sw=4:
