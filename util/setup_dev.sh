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

mkdir -p $DIR_CPANM

pushd $DIR_CPANM
    if [[ ! -x "cpanm" ]]; then
        download cpanm https://cpanmin.us/
        chmod +x cpanm
    fi

    ./cpanm --notest --local-lib=$DIR_CPANM local::lib
    eval $(perl -I$DIR_CPANM -Mlocal::lib)

    ./cpanm --notest --local-lib=$DIR_CPANM Test::Nginx
    ./cpanm --notest --local-lib=$DIR_CPANM IPC::Run
popd

# vim: ft=sh st=4 sts=4 sw=4:
