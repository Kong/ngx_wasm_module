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

show_usage() {
    cat << EOF
Usage:

  $SCRIPT_NAME <name> --src
  $SCRIPT_NAME <name> --bin [--ngx <ver>] [--wasmtime <ver>] [--wasmer <ver>] [--v8 <ver>]
  $SCRIPT_NAME <name> --all [--match <pattern>]

EOF
}

show_help() {
    cat << EOF
Generate release assets in: $DIR_DIST_OUT

EOF
    show_usage
    cat << EOF
Arguments:

  <name>  Release name.
          Will appear in asset names as wasmx-NAME-RUNTIME-ARCH-OS.EXT

  Release mode:

  --src   Produce source tarball.

  --bin   Produce binary package(s) for the currently running system.
          At least one runtime must be specified either via the
          --wasmtime, --wasmer, and/or --v8 arguments, or by setting the
          WASMTIME_VER, WASMER_V8, and/or V8_VER environment variables.

  --all   Produce source tarball and binary packages for all
          supported runtimes and distros using container images.

Options:

  NOTE: --all will infer all unspecified <ver> arguments from the Makefile.

  --ngx <ver>         Nginx version.
                      Inferred from the Makefile if unspecified.

  --wasmtime <ver>    Set Wasmtime version.

  --wasmer <ver>      Set Wasmer version.

  --v8 <ver>          Set V8 version.

  --match <pattern>   (--all only) Only container images whose names
                      match <pattern> will be used.

  -h, --help          Print this message and exit.

EOF
}

###############################################################################

while (( "$#" )); do
    case "$1" in
        -h|--help)
            show_help
            exit 0
            ;;
        --src)
            RELEASE_MODE=src
            shift
            ;;
        --all|--bin-all) # --bin-all: deprecated
            RELEASE_MODE=all
            shift
            ;;
        --bin)
            RELEASE_MODE=bin
            shift
            ;;
        --match)
            MATCH_DOCKERFILES="$2"
            if [ -z "$MATCH_DOCKERFILES" ]; then
                show_usage
                fatal "--match argument missing a pattern to match Dockerfiles"
            fi
            shift 2
            ;;
        --ngx)
            NGX_VER="$2"
            if [ -z "$NGX_VER" ]; then
                show_usage
                fatal "--ngx argument missing version"
            fi
            shift 2
            ;;
        --wasmtime)
            WASMTIME_VER="$2"
            if [ -z "$WASMTIME_VER" ]; then
                show_usage
                fatal "--wasmtime argument missing version"
            fi
            shift 2
            ;;
        --wasmer)
            WASMER_VER="$2"
            if [ -z "$WASMER_VER" ]; then
                show_usage
                fatal "--wasmer argument missing version"
            fi
            shift 2
            ;;
        --v8)
            V8_VER="$2"
            if [ -z "$V8_VER" ]; then
                show_usage
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
        show_usage
        fatal "$SCRIPT_NAME missing name argument/env variable"
    fi
fi

DIST_SRC="ngx_wasm_module-$name"
DIR_BUILD_DOCKERFILES=$NGX_WASM_DIR/assets/release/Dockerfiles
DIR_BUILD=$DIR_DIST_WORK/build
DIR_DIST_SRC=$DIR_DIST_WORK/$DIST_SRC

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

build_static_binary() {
    local arch=$1
    local runtime=$2
    local runtime_ver=$3

    local dist_bin_name=wasmx-$name-$runtime-$arch-$(get_distro)

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

    pushd nginx-$NGX_VER

    export NGX_WASM_RUNTIME=$runtime

    ./configure \
        --build="wasmx $name [vm: $NGX_WASM_RUNTIME]" \
        --builddir="$DIR_BUILD/build-$dist_bin_name" \
        --with-cc-opt="-Wno-error -g -O3 $CC_FLAGS" \
        --with-ld-opt="$LD_FLAGS" \
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

    popd

    notice "Creating binary archive..."
    pushd $DIR_DIST_WORK
    cp $DIR_BUILD/build-$dist_bin_name/nginx \
       $DIST_SRC/assets/release/nginx.conf \
       $DIST_SRC/assets/release/README \
       $DIST_SRC/LICENSE \
       $dist_bin_name
    tar czf $dist_bin_name.tar.gz $dist_bin_name
    mv $dist_bin_name.tar.gz $DIR_DIST_OUT
    notice "Created $DIR_DIST_OUT/$dist_bin_name.tar.gz"
    popd
}

build_with_runtime() {
    local runtime="$1"
    local version="$2"
    local arch="$3"
    local libname="$4"

    local distro="$(get_distro)"
    local runtime_dir="$DIR_DIST_WORK/$runtime-$version-$distro"
    local mode="--download"

    if [ $runtime = v8 ]; then
        mode="--build"
    fi

    eval $NGX_WASM_DIR/util/runtime.sh \
        --runtime $runtime \
        --runtime-version "$version" \
        --target-dir "$runtime_dir" \
        --arch "$arch" \
        --clean \
        $mode

    local save_CC_FLAGS="$CC_FLAGS"
    local save_LD_FLAGS="$LD_FLAGS"

    if [ -n "$libname" ]; then
        CC_FLAGS="-I$runtime_dir/include"
        LD_FLAGS="$LD_FLAGS $runtime_dir/lib/$libname"
    fi

    export NGX_WASM_RUNTIME_INC="$runtime_dir/include"
    export NGX_WASM_RUNTIME_LIB="$runtime_dir/lib"

    build_static_binary $arch $runtime $version

    CC_FLAGS="$save_CC_FLAGS"
    LD_FLAGS="$save_LD_FLAGS"
}

# ngx_wasm_module-$name.tar.gz (sources)

release_source() {
    notice "Creating source archive..."
    pushd $DIR_DIST_WORK
    tar czf $DIST_SRC.tar.gz \
        $DIST_SRC/config \
        $DIST_SRC/auto \
        $DIST_SRC/src \
        $DIST_SRC/lib/resty \
        $DIST_SRC/LICENSE \
        $DIST_SRC/assets/release/INSTALL
    cp $DIST_SRC.tar.gz $DIR_DIST_OUT
    popd
    notice "Created $DIR_DIST_OUT/$DIST_SRC.tar.gz"
}

# wasmx-$name-$runtime-$arch-$os.tar.gz (binary)

release_bin() {
    local arch=$(uname -m)

    if [ -z "$NGX_VER" ]; then
        show_usage
        fatal "$SCRIPT_NAME missing Nginx version for static build, specify --ngx <ver>"
    fi

    if [ -z "$OPENSSL_VER" ]; then
        fatal "$SCRIPT_NAME missing OpenSSL version for static build"
    fi

    if [ -z "$PCRE_VER" ]; then
        fatal "$SCRIPT_NAME missing PCRE version for static build"
    fi

    if [ -z "$ZLIB_VER" ]; then
        fatal "$SCRIPT_NAME missing zlib version for static build"
    fi

    notice "Building $arch binary..."

    if [ "$(get_distro)" = "centos7" ]; then
        notice "Enabling devtoolset-8 for CentOS..."
        source /opt/rh/devtoolset-8/enable
        gcc --version
        export CC=gcc
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
        fatal "unreachable: missing wasm runtime, specify --wasmer, --wasmtime, or --v8"
    fi
}

release_all_bin_docker() {
    if [ ! -x "$(command -v docker)" ]; then
        fatal "missing 'docker' command"
    fi

    if [ -z "$WASMTIME_VER" ]; then
        WASMTIME_VER=$(get_variable_from_makefile WASMTIME)
    fi

    if [ -z "$WASMER_VER" ]; then
        WASMER_VER=$(get_variable_from_makefile WASMER)
    fi

    if [ -z "$V8_VER" ]; then
        V8_VER=$(get_variable_from_makefile V8)
    fi

    dockerfiles=`find $DIR_BUILD_DOCKERFILES -type f -name 'Dockerfile.*' \
                 | grep "$MATCH_DOCKERFILES"`

    for path in $dockerfiles; do
        local dockerfile=$(basename $path)
        local imgname=${dockerfile#"Dockerfile."}
        local imgtag=wasmx-build-$imgname

        docker build \
            --rm \
            -f $path \
            -t $imgtag:latest \
            $DIR_DIST_SRC

        docker rm -f $imgtag
        docker run \
            -it \
            --entrypoint /bin/sh \
            --name $imgtag \
            -e RELEASE_NAME=$name \
            -e NGX_VER=$NGX_VER \
            -e WASMTIME_VER=$WASMTIME_VER \
            -e WASMER_VER=$WASMER_VER \
            -e V8_VER=$V8_VER \
            $imgtag \
            -c "/ngx_wasm_module/util/release.sh --bin"

         docker cp \
             $imgtag:/ngx_wasm_module/dist \
             $DIR_DIST_WORK

         docker rm -f $imgtag

         cp $DIR_DIST_WORK/dist/*.tar.gz $DIR_DIST_OUT
         rm -rf $DIR_DIST_WORK/dist
    done
}

# main

mkdir -p $DIR_DIST_WORK $DIR_DIST_OUT $DIR_DIST_SRC

pushd $DIR_DIST_WORK

# prepare sources

rm -rf $DIR_DIST_SRC/*
cp -R \
    $NGX_WASM_DIR/rust-toolchain \
    $NGX_WASM_DIR/Makefile \
    $NGX_WASM_DIR/LICENSE \
    $NGX_WASM_DIR/config \
    $NGX_WASM_DIR/auto \
    $NGX_WASM_DIR/src \
    $NGX_WASM_DIR/lib \
    $NGX_WASM_DIR/util \
    $NGX_WASM_DIR/assets \
    $DIR_DIST_SRC

# produce release artifact

if [ "$RELEASE_MODE" = "src" ]; then
    release_source

elif [ "$RELEASE_MODE" = "all" ]; then
    release_source

    case $OSTYPE in
        linux*)  release_all_bin_docker;;
        *)       fatal "cannot build release from docker images on \"$OSTYPE\""
    esac

elif [ "$RELEASE_MODE" = "bin" ]; then
    case $OSTYPE in
        linux*)  release_bin;;
        darwin*) release_bin;;
        *)       fatal "unsupported OS \"$OSTYPE\""
    esac

else
    fatal "missing release mode, specify --src, --bin or --all"
fi

# vim: ft=sh ts=4 sts=4 sw=4:
