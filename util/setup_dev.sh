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

mkdir -p $DIR_CPANM $DIR_BIN $DIR_TESTS_LIB_WASM

pushd $DIR_CPANM
    if [[ ! -x "cpanm" ]]; then
        notice "downloading cpanm..."
        download cpanm https://cpanmin.us/
        chmod +x cpanm
    fi

    notice "downloading Test::Nginx dependencies..."
    ./cpanm --notest --local-lib=$DIR_CPANM local::lib
    ./cpanm --notest --local-lib=$DIR_CPANM Test::Nginx
    ./cpanm --notest --local-lib=$DIR_CPANM IPC::Run
popd

pushd $DIR_BIN
    notice "downloading the reindex script..."
    download reindex https://raw.githubusercontent.com/openresty/openresty-devel-utils/master/reindex
    chmod +x reindex
popd

if [[ -d "$DIR_ECHO" ]]; then
    notice "updating the echo-nginx-module repository..."
    pushd $DIR_ECHO
        git fetch
        git reset --hard origin/master
    popd
else
    notice "cloning the echo-nginx-module repository..."
    git clone https://github.com/openresty/echo-nginx-module.git $DIR_ECHO
fi

get_no_pool_nginx 1

notice "done"

# vim: ft=sh st=4 sts=4 sw=4:
