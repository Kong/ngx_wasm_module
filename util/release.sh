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
PCRE_VER=8.44
OPENSSL_VER=1.1.1k
ZLIB_VER=1.2.11

arch=$(uname -m)

while (( "$#" )); do
    case "$1" in
        --static)
            RELEASE_STATIC=1
            shift
            ;;
        --ngx)
            NGX_VER="$2"
            shift 2
            ;;
        --wasmtime)
            WASMTIME_VER="$2"
            shift 2
            ;;
        --wasmer)
            WASMER_VER="$2"
            shift 2
            ;;
        *)
            PARAMS="$PARAMS $1"
            shift
            ;;
    esac
done

eval set -- "$PARAMS"

name=$1

if [ -z "$name" ]; then
    fatal "$SCRIPT_NAME missing <release-name> argument"
fi

DIR_DIST_WORK=$DIR_WORK/ngx_wasm_module_dist
DIST_SRC=ngx_wasm_module-$name
DIR_BUILD=$DIR_DIST_WORK/build

mkdir -p $DIR_DIST_WORK $DIR_DIST_OUT
cd $DIR_DIST_WORK

if [[ -d $DIST_SRC || -d $DIST_BIN ]]; then
    rm -rf $DIST_SRC $DIST_BIN
fi

mkdir -p $DIR_DIST_OUT $DIST_SRC $DIST_BIN

# ngx_wasm_module-$name.tar.gz (sources)

echo "Creating source archive..."
cp -R \
    $NGX_WASM_DIR/config \
    $NGX_WASM_DIR/src \
    $DIST_SRC

tar czf $DIST_SRC.tar.gz $DIST_SRC
mv $DIST_SRC.tar.gz $DIR_DIST_OUT
echo "Created $DIR_DIST_OUT/$DIST_SRC.tar.gz"

# nginx binary (static)

if [ -z "$RELEASE_STATIC" ]; then
    exit
fi

if [ -z "$NGX_VER" ]; then
    fatal "$SCRIPT_NAME missing --ngx <ver> argument for static build"
fi

echo "Building statically linked binary..."
cd $DIR_DIST_WORK

download nginx-$NGX_VER.tar.gz \
    "https://nginx.org/download/nginx-$NGX_VER.tar.gz"

if [ ! -d "nginx-$NGX_VER" ]; then
    tar -xf nginx-$NGX_VER.tar.gz
fi

if [ -n "$WASMTIME_VER" ]; then
    download_wasmtime $WASMTIME_VER

    export NGX_WASM_RUNTIME=wasmtime
    export NGX_WASM_RUNTIME_INC="$(pwd)/wasmtime-$WASMTIME_VER/include"
    export NGX_WASM_RUNTIME_LD_OPT="$(pwd)/wasmtime-$WASMTIME_VER/lib/libwasmtime.a -lm"

elif [ -n "$WASMER_VER" ]; then
    download_wasmer $WASMER_VER

    export NGX_WASM_RUNTIME=wasmer
    export NGX_WASM_RUNTIME_INC="$(pwd)/wasmer-$WASMER_VER/include"
    export NGX_WASM_RUNTIME_LD_OPT="$(pwd)/wasmer-$WASMER_VER/lib/libwasmer.a -lm"

else
    fatal "$SCRIPT_NAME missing --wasmtime/--wasmer <ver> argument for static build"
fi

DIST_BIN=wasmx-$name-$NGX_WASM_RUNTIME-$arch

# openssl

download openssl-$OPENSSL_VER.tar.gz \
    "https://www.openssl.org/source/openssl-$OPENSSL_VER.tar.gz"

if [ ! -d "openssl-$OPENSSL_VER" ]; then
    tar -xf openssl-$OPENSSL_VER.tar.gz
fi

# pcre

download pcre-$PCRE_VER.tar.gz \
    "https://ftp.pcre.org/pub/pcre/pcre-$PCRE_VER.tar.gz"

if [ ! -d "pcre-$PCRE_VER" ]; then
    tar -xf pcre-$PCRE_VER.tar.gz
fi

# zlib

download zlib-$ZLIB_VER.tar.gz \
    "https://www.zlib.net/zlib-$ZLIB_VER.tar.gz"

if [ ! -d "zlib-$ZLIB_VER" ]; then
    tar -xf zlib-$ZLIB_VER.tar.gz
fi

# build

cd nginx-$NGX_VER

./configure \
    --build="wasmx $name" \
    --builddir=$DIR_BUILD \
    --with-cc-opt='-O3' \
    --with-ld-opt='-static-libgcc' \
    --add-module="$DIR_DIST_WORK/$DIST_SRC" \
    --with-openssl="$DIR_DIST_WORK/openssl-$OPENSSL_VER" \
    --with-zlib="$DIR_DIST_WORK/zlib-$ZLIB_VER" \
    --with-pcre="$DIR_DIST_WORK/pcre-$PCRE_VER" \
    --with-pcre-jit

make -j${n_jobs}

if nm -g "$DIR_BUILD/nginx" | grep -qv "T wasm_instance_new"; then
    fatal "$NGX_WASM_RUNTIME lib incorrectly linked"
fi

cd $DIR_DIST_WORK
cp $DIR_BUILD/nginx \
   $NGX_WASM_DIR/misc/nginx.conf \
   $NGX_WASM_DIR/misc/README \
    $DIST_BIN

tar czf $DIST_BIN.tar.gz $DIST_BIN
mv $DIST_BIN.tar.gz $DIR_DIST_OUT
echo "Created $DIR_DIST_OUT/$DIST_BIN.tar.gz"

# vim: ft=sh st=4 sts=4 sw=4:
