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
set +e

name=$1

if [ -z "$name" ]; then
     fatal "$SCRIPT_NAME missing <release-name> argument"
fi

DIR_DIST=$DIR_WORK/ngx_wasm_module_dist

mkdir -p $DIR_DIST $DIR_DIST_OUT
cd $DIR_DIST
rm -rf *

cp -R \
    $NGX_WASM_DIR/config \
    $NGX_WASM_DIR/auto \
    $NGX_WASM_DIR/src \
    .

tar czf ngx_wasm_module-$name.tar.gz *

mv *.tar.gz $DIR_DIST_OUT

echo "Created $DIR_DIST_OUT/ngx_wasm_module-$name.tar.gz"

# vim: ft=sh st=4 sts=4 sw=4:
