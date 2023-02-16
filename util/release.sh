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
NGX_VER=${NGX_VER:-$(get_variable_from_makefile NGX)}
OPENSSL_VER=${OPENSSL_VER:-$(get_variable_from_makefile OPENSSL)}
PCRE_VER=${PCRE_VER:-$(get_variable_from_makefile PCRE)}
ZLIB_VER=${ZLIB_VER:-$(get_variable_from_makefile ZLIB)}

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
            if [ -z "$NGX_VER" ]; then
                fatal "--ngx argument missing version"
            fi
            shift 2
            ;;
        --wasmtime)
            WASMTIME_VER="$2"
            if [ -z "$WASMTIME_VER" ]; then
                fatal "--wasmtime argument missing version"
            fi
            shift 2
            ;;
        --wasmer)
            WASMER_VER="$2"
            if [ -z "$WASMER_VER" ]; then
                fatal "--wasmer argument missing version"
            fi
            shift 2
            ;;
        --v8)
            V8_VER="$2"
            if [ -z "$V8_VER" ]; then
                fatal "--v8 argument missing version"
            fi
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

DIR_BUILD_DOCKERFILES=$NGX_WASM_DIR/assets/release/Dockerfiles
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
       $NGX_WASM_DIR/assets/release/INSTALL \
       $DIST_SRC

    mkdir -p $DIST_SRC/lib
    cp -R \
       $NGX_WASM_DIR/lib/resty \
       $DIST_SRC/lib

    tar czf $DIST_SRC.tar.gz $DIST_SRC
    cp $DIST_SRC.tar.gz $DIR_DIST_OUT
    notice "Created $DIR_DIST_OUT/$DIST_SRC.tar.gz"
}

get_distro() {
    local distro

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
        arch)    distro='archlinux';;
        ubuntu|centos)
            if [ -n $VERSION_ID ]; then
                distro=$distro$VERSION_ID
            fi
            ;;
    esac

    echo "$distro"
}

# nginx binary (static)

build_static_binary() {
    local arch=$1
    local runtime=$2
    local runtime_ver=$3

    if [ ! -d $DIST_SRC ]; then
        fatal "missing source release at $(pwd)/$DIST_SRC to build binary, run with --src"
    fi

    local dist_bin_name=wasmx-$name-$runtime-$arch-$(get_distro)

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
        "https://downloads.sourceforge.net/project/pcre/pcre/$PCRE_VER/pcre-$PCRE_VER.tar.gz"

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
        --with-openssl-opt="no-tests" \
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
        --without-http_fastcgi_module || cat $DIR_BUILD/build-$dist_bin_name/autoconf*

    make -j$(n_jobs)

    cd $DIR_DIST_WORK
    cp $DIR_BUILD/build-$dist_bin_name/nginx \
       $NGX_WASM_DIR/assets/release/nginx.conf \
       $NGX_WASM_DIR/assets/release/README \
       $dist_bin_name

    tar czf $dist_bin_name.tar.gz $dist_bin_name
    mv $dist_bin_name.tar.gz $DIR_DIST_OUT
    notice "Created $DIR_DIST_OUT/$dist_bin_name.tar.gz"
}

build_with_runtime() {
    local runtime="$1"
    local version="$2"
    local arch="$3"
    local libname="$4"

    local distro="$(get_distro)"
    local runtime_dir="$DIR_WORK/$runtime-$version-$distro"

    $NGX_WASM_DIR/util/runtime.sh \
        --runtime $runtime \
        --runtime-version "$version" \
        --target-dir "$runtime_dir" \
        --arch "$arch" \
        --build \
        --clean

    local save_CC_FLAGS="$CC_FLAGS"
    local save_LD_FLAGS="$LD_FLAGS"

    if [ -n "$libname" ]; then
        CC_FLAGS="-I$runtime_dir/include"
        LD_FLAGS="$runtime_dir/lib/$libname -ldl -lpthread $LD_FLAGS"
    fi

    export NGX_WASM_RUNTIME_INC="$runtime_dir/include"
    export NGX_WASM_RUNTIME_LIB="$runtime_dir/lib"

    build_static_binary $arch $runtime $version

    CC_FLAGS="$save_CC_FLAGS"
    LD_FLAGS="$save_LD_FLAGS"
}

release_bin() {
    local arch=$(uname -m)

    notice "Building $arch binary..."

    if [ "$(get_distro)" = "centos7" ]; then
        notice "Enabling devtoolset-8 for CentOS..."
        source /opt/rh/devtoolset-8/enable
        gcc --version
    fi

    if [ -n "$WASMTIME_VER" ]; then
        build_with_runtime wasmtime $WASMTIME_VER $arch "libwasmtime.a"
    fi

    if [ -n "$WASMER_VER" ]; then
        build_with_runtime wasmer $WASMER_VER $arch "libwasmer.a"
    fi

    if [ -n "$V8_VER" ]; then
        build_with_runtime v8 $V8_VER $arch ""
    fi

    if [[ -z "$WASMTIME_VER" && -z "$WASMER_VER" && -z "$V8_VER" ]]; then
        fatal "missing wasm vm, specify --wasmer, --wasmtime, or --v8"
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
            -e V8_VER=$V8_VER \
            -e RELEASE_NAME=$name \
            $imgtag \
            -c "./ngx_wasm_module/util/release.sh --bin"
    done
}

if [ -n "$RELEASE_SOURCE" ]; then
    release_source

elif [ -n "$RELEASE_BIN_ALL" ]; then
    release_source

    case $OSTYPE in
        linux*)  release_all_bin_docker;;
        *)       fatal "cannot build release from docker images on \"$OSTYPE\""
    esac

elif [ -n "$RELEASE_BIN" ]; then
    release_source

    case $OSTYPE in
        linux*)  release_bin;;
        darwin*) release_bin;;
        *)       fatal "unsupported OS \"$OSTYPE\""
    esac

else
    fatal "missing release type, specify --bin, --bin-all, or --src"
fi

# vim: ft=sh ts=4 sts=4 sw=4:
