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

while (( "$#" )); do
    case "$1" in
        --src)
            RELEASE_SOURCE=1
            shift
            ;;
        --bin-all)
            RELEASE_BIN_ALL=1
            shift
            ;;
        --bin)
            RELEASE_BIN=1
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
    name=$RELEASE_NAME

    if [ -z "$name" ]; then
        fatal "$SCRIPT_NAME missing release name argument/env variable"
    fi
fi

DIR_BUILD_DOCKERFILES=$NGX_WASM_DIR/util/Dockerfiles
DIR_DIST_WORK=$DIR_WORK/ngx_wasm_module_dist
DIR_BUILD=$DIR_DIST_WORK/build
DIST_SRC="ngx_wasm_module-$name"

mkdir -p $DIR_DIST_WORK $DIR_DIST_OUT
cd $DIR_DIST_WORK

# ngx_wasm_module-$name.tar.gz (sources)

release_source() {
    notice "Creating source archive..."
    cd $DIR_DIST_WORK

    if [ -d $DIST_SRC ]; then
        rm -rf $DIST_SRC
    fi

    mkdir -p $DIST_SRC
    cp -R \
        $NGX_WASM_DIR/config \
        $NGX_WASM_DIR/auto \
        $NGX_WASM_DIR/src \
        $NGX_WASM_DIR/misc/INSTALL \
        $DIST_SRC

    tar czf $DIST_SRC.tar.gz $DIST_SRC
    cp $DIST_SRC.tar.gz $DIR_DIST_OUT
    notice "Created $DIR_DIST_OUT/$DIST_SRC.tar.gz"
}

# nginx binary (static)

build_static_binary() {
    local arch=$1
    local runtime=$2
    local runtime_ver=$3
    local distro

    if [ ! -d $DIST_SRC ]; then
        fatal "missing source release at $(pwd)/$DIST_SRC to build binary, run with --src"
    fi

    if [ -f /etc/os-release ]; then
        . /etc/os-release
        distro=$ID

    elif type lsb_release >/dev/null 2>&1; then
        distro=$(lsb_release -si)

    else
        distro=$OSTYPE
    fi

    case $distro in
        darwin*) distro='macos';;
        arch)   distro='archlinux';;
        ubuntu|centos)
            if [ -n $VERSION_ID ]; then
                distro=$distro$VERSION_ID
            fi
            ;;
    esac

    local dist_bin_name=wasmx-$name-$runtime-$arch-$distro

    if [ -z "$NGX_VER" ]; then
        fatal "$SCRIPT_NAME missing nginx version for static build, specify --ngx <ver>"
    fi

    cd $DIR_DIST_WORK
    notice "Building statically linked binary $dist_bin_name..."
    mkdir -p $dist_bin_name

    download nginx-$NGX_VER.tar.gz \
        "https://nginx.org/download/nginx-$NGX_VER.tar.gz"

    if [ ! -d "nginx-$NGX_VER" ]; then
        tar -xf nginx-$NGX_VER.tar.gz
    fi

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

    export NGX_WASM_RUNTIME=$runtime

    ./configure \
        --build="wasmx $name [vm: $NGX_WASM_RUNTIME]" \
        --builddir="$DIR_BUILD/build-$dist_bin_name" \
        --with-cc-opt="-Wno-error -g -O3 $CC_FLAGS" \
        --with-ld-opt="-lm $LD_FLAGS" \
        --prefix='.' \
        --conf-path='nginx.conf' \
        --pid-path='nginx.pid' \
        --sbin-path='nginx' \
        --error-log-path='error.log' \
        --http-log-path='access.log' \
        --http-client-body-temp-path='client_body_temp' \
        --add-module="$DIR_DIST_WORK/$DIST_SRC" \
        --with-openssl="$DIR_DIST_WORK/openssl-$OPENSSL_VER" \
        --with-zlib="$DIR_DIST_WORK/zlib-$ZLIB_VER" \
        --with-pcre="$DIR_DIST_WORK/pcre-$PCRE_VER" \
        --with-pcre-jit \
        --with-stream \
        --with-stream_ssl_module \
        --with-stream_ssl_preread_module \
        --with-http_ssl_module \
        --with-http_v2_module \
        --with-http_stub_status_module \
        --with-http_gzip_static_module \
        --with-http_gunzip_module \
        --with-http_realip_module \
        --with-http_addition_module \
        --with-http_secure_link_module \
        --with-http_auth_request_module \
        --with-http_random_index_module \
        --with-http_sub_module \
        --with-http_dav_module \
        --with-http_flv_module \
        --with-http_mp4_module \
        --with-threads \
        --without-mail_pop3_module \
        --without-mail_imap_module \
        --without-mail_smtp_module \
        --without-http_scgi_module \
        --without-http_uwsgi_module \
        --without-http_fastcgi_module

    make -j${n_jobs}

    cd $DIR_DIST_WORK
    cp $DIR_BUILD/build-$dist_bin_name/nginx \
       $NGX_WASM_DIR/misc/nginx.conf \
       $NGX_WASM_DIR/misc/README \
        $dist_bin_name

    tar czf $dist_bin_name.tar.gz $dist_bin_name
    mv $dist_bin_name.tar.gz $DIR_DIST_OUT
    notice "Created $DIR_DIST_OUT/$dist_bin_name.tar.gz"
}

release_bin() {
    local arch=$(uname -m)
    case $arch in
        x86_64)  arch='amd64';;
        aarch64) arch='arm64';;
        *)       arch=$arch
    esac

    if [ -n "$WASMTIME_VER" ]; then
        download_wasmtime $WASMTIME_VER

        CC_FLAGS="-I$(pwd)/wasmtime-$WASMTIME_VER/include"
        LD_FLAGS="$(pwd)/wasmtime-$WASMTIME_VER/lib/libwasmtime.a -ldl -lpthread"

        build_static_binary $arch wasmtime $WASMTIME_VER

    elif [ -n "$WASMER_VER" ]; then
        download_wasmer $WASMER_VER $arch

        CC_FLAGS="-I$(pwd)/wasmer-$WASMER_VER/include"
        LD_FLAGS="$(pwd)/wasmer-$WASMER_VER/lib/libwasmer.a -ldl -lpthread"

        build_static_binary $arch wasmer $WASMER_VER

    else
        fatal "missing wasm vm, specify --wasmer or --wasmtime"
    fi
}

release_all_bin_docker() {
    if [ ! -x "$(command -v docker)" ]; then
        fatal "missing 'docker' command"
    fi

    for path in $DIR_BUILD_DOCKERFILES/Dockerfile.*; do
        local dockerfile=$(basename $path)
        local imgname=${dockerfile#"Dockerfile."}
        local imgtag=wasmx-build-$imgname

        docker build \
            -t $imgtag \
            -f $path $DIR_BUILD_DOCKERFILES

        docker run \
            --rm -it \
            --entrypoint /bin/sh \
            -v "$NGX_WASM_DIR:/ngx_wasm_module" \
            -u $(id -u ${USER}):$(id -g ${USER}) \
            -e NGX_VER=$NGX_VER \
            -e WASMTIME_VER=$WASMTIME_VER \
            -e WASMER_VER=$WASMER_VER \
            -e RELEASE_NAME=$name \
            $imgtag \
            -c "./ngx_wasm_module/util/release.sh --bin"
    done
}

if [ -n "$RELEASE_SOURCE" ]; then
    release_source
fi

if [ -n "$RELEASE_BIN_ALL" ]; then
    case $OSTYPE in
        linux*)  release_all_bin_docker;;
        *)       fatal "cannot build release from docker images on \"$OSTYPE\""
    esac

elif [ -n "$RELEASE_BIN" ]; then
    case $OSTYPE in
        linux*)  release_bin;;
        darwin*) release_bin;;
        *)       fatal "unsupported OS \"$OSTYPE\""
    esac

else
    fatal "missing release type, specify --bin, --bin-all, or --src"
fi

# vim: ft=sh st=4 sts=4 sw=4:
