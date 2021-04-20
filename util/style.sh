#!/usr/bin/env bash

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

if [[ ! -x "$DIR_BIN/style" ]]; then
    fatal "missing 'style' script, run 'make setup' first"
fi

FILE_OUT=$DIR_WORK/style.out
rc=0

$DIR_BIN/style "$@" >$FILE_OUT &
wait

if [[ -s "$FILE_OUT" ]]; then
    cat $FILE_OUT
    rc=1
fi

rm -f $FILE_OUT

exit $rc
