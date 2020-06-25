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

export PERL5LIB=$DIR_CPANM/lib/perl5:$PERL5LIB

export TEST_NGINX_BINARY=${TEST_NGINX_BINARY:=$DIR_BUILDROOT/nginx}
export TEST_NGINX_SLEEP=${TEST_NGINX_SLEEP:=0.002}
export TEST_NGINX_PORT=${TEST_NGINX_PORT:=1984}
export TEST_NGINX_TIMEOUT=${TEST_NGINX_TIMEOUT:=10}
export TEST_NGINX_RESOLVER=${TEST_NGINX_RESOLVER:=8.8.4.4}
#export TEST_NGINX_RANDOMIZE=${TEST_NGINX_RANDOMIZE:=}
#export TEST_NGINX_CHECK_LEAK=
#export TEST_NGINX_USE_VALGRIND=
#export TEST_NGINX_USE_STAP=
#export TEST_NGINX_USE_HUP=
#export TEST_NGINX_EVENT_TYPE=poll
#export LD_PRELOAD=

#if [[ ! -z "$TEST_NGINX_USE_VALGRIND" ]]; then
    #export LD_LIBRARY_PATH=/:$LD_LIBRARY_PATH
#fi

#if [[ ! -z "$TEST_NGINX_USE_STAP" ]]; then
    #export PATH=/usr/local/opt/systemtap/bin:$PATH
#fi

if [[ ! -x "$TEST_NGINX_BINARY" ]]; then
    fatal "no nginx binary at $TEST_NGINX_BINARY"
fi

echo $TEST_NGINX_BINARY
echo
eval "$TEST_NGINX_BINARY -V"
echo

if [[ $(uname -s) == Darwin ]]; then
    otool -L $TEST_NGINX_BINARY | grep wasm
else
    ldd $TEST_NGINX_BINARY | grep wasm
fi

echo

exec prove "$@"

# vim: ft=sh st=4 sts=4 sw=4:
