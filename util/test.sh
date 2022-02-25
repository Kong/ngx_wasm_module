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

while (( "$#" )); do
    case "$1" in
        --no-test-nginx)
            NO_TEST_NGINX=1
            shift
            ;;
        -j)
            export TEST_NGINX_RANDOMIZE=1
            shift
            ;;
        *)
            PARAMS="$PARAMS $1"
            shift
            ;;
    esac
done

eval set -- "$PARAMS"

if [[ ! -d "$DIR_CPANM/lib/perl5" ]]; then
    fatal "missing $DIR_CPANM/lib/perl5 lib, run 'make setup' first"
fi

export PERL5LIB=$DIR_CPANM/lib/perl5:$PERL5LIB

# Test::Build

if [[ "$NO_TEST_NGINX" == 1 ]]; then
    exec prove $@
fi

# Test::Nginx

export TEST_NGINX_SERVROOT=${TEST_NGINX_SERVROOT:=$DIR_PREFIX}
export TEST_NGINX_BINARY=${TEST_NGINX_BINARY:=$DIR_BUILDROOT/nginx}
export TEST_NGINX_SLEEP=${TEST_NGINX_SLEEP:=0.2}
export TEST_NGINX_PORT=${TEST_NGINX_PORT:=1984}
export TEST_NGINX_TIMEOUT=${TEST_NGINX_TIMEOUT:=45}
export TEST_NGINX_RESOLVER=${TEST_NGINX_RESOLVER:=8.8.4.4}
export ASAN_OPTIONS=detect_odr_violation=0   # see asan.ignore (clang 13)
export LSAN_OPTIONS="suppressions=$NGX_WASM_DIR/asan.suppress"
#export TEST_NGINX_NO_SHUFFLE=1
#export TEST_NGINX_RANDOMIZE=1
#export TEST_NGINX_LOAD_MODULES=
#export TEST_NGINX_MASTER_PROCESS=
#export TEST_NGINX_CHECK_LEAK=1
#export TEST_NGINX_USE_HUP=1
#export TEST_NGINX_USE_VALGRIND=1
#export TEST_NGINX_USE_STAP=1
#export TEST_NGINX_USE_RR=1
#export TEST_NGINX_NO_CLEAN=1
#export TEST_NGINX_BENCHMARK='1000 10'
#export LD_PRELOAD=

if [[ ! -x "$TEST_NGINX_BINARY" ]]; then
    fatal "no nginx binary at $TEST_NGINX_BINARY"
fi

if [[ ! -x "$(command -v cargo)" ]]; then
    fatal "missing 'cargo', is the rust toolchain installed?"
fi

if [[ "$CI" == 'true' ]]; then
    cargo clean
    export NGX_BUILD_FORCE=1
fi

cargo build \
    --lib \
    --release \
    --target wasm32-wasi \
    --out-dir $DIR_TESTS_LIB_WASM \
    -Z unstable-options

if [ $(uname -s) = Linux ]; then
    export TEST_NGINX_EVENT_TYPE=epoll
fi

if [[ "$TEST_NGINX_USE_VALGRIND" == 1 ]]; then
    export TEST_NGINX_SLEEP=3
    echo "TEST_NGINX_USE_VALGRIND=$TEST_NGINX_USE_VALGRIND"
    echo "TEST_NGINX_SLEEP=$TEST_NGINX_SLEEP"
    echo "TEST_NGINX_TIMEOUT=$TEST_NGINX_TIMEOUT"
    valgrind --version
    echo
fi

if [[ ! -z "$TEST_NGINX_USE_HUP" ]]; then
    echo "TEST_NGINX_USE_HUP=$TEST_NGINX_USE_HUP"
fi

if [[ "$TEST_NGINX_RANDOMIZE" == 1 ]]; then
    prove_opts="-j$(n_jobs)"
    echo "TEST_NGINX_RANDOMIZE=$TEST_NGINX_RANDOMIZE ($prove_opts)"
fi

echo
echo $TEST_NGINX_BINARY
echo
eval "$TEST_NGINX_BINARY -V"
echo

if [ $(uname -s) = Darwin ]; then
    otool -L $TEST_NGINX_BINARY | grep wasm

elif [ ! -x "$(command -v ldd)" ]; then
    ldd $TEST_NGINX_BINARY | grep wasm
fi

echo
exec prove -r $prove_opts $@

# vim: ft=sh st=4 sts=4 sw=4:
