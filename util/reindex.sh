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

if [[ ! -x "$DIR_BIN/reindex" ]]; then
    fatal "missing 'reindex' script, run 'make setup' first"
fi

exec $DIR_BIN/reindex "$@"

# vim: ft=sh ts=4 sts=4 sw=4:
