DIR_WORK=$NGX_WASM_DIR/work
DIR_BIN=$DIR_WORK/bin
DIR_DOWNLOAD=$DIR_WORK/downloads
DIR_CPANM=$DIR_DOWNLOAD/cpanm
DIR_NOPOOL=$DIR_DOWNLOAD/no-pool-nginx
DIR_NGX_ECHO_MODULE=$DIR_DOWNLOAD/echo-nginx-module
DIR_NGX_HEADERS_MORE_MODULE=$DIR_DOWNLOAD/headers-more-nginx-module
DIR_MOCKEAGAIN=$DIR_DOWNLOAD/mockeagain
DIR_BUILDROOT=$DIR_WORK/buildroot
DIR_SRCROOT=$DIR_WORK/nginx-patched
DIR_TESTS_LIB_WASM=$DIR_WORK/lib/wasm
DIR_PREFIX=$NGX_WASM_DIR/t/servroot
DIR_DIST_OUT=$NGX_WASM_DIR/dist

build_nginx() {
    local ngx_src=$1
    local ngx_ver=$2
    local build_name=(dev)
    local build_opts=()

    NGX_BUILD_DIR_SRCROOT="${NGX_BUILD_DIR_SRCROOT:-$DIR_SRCROOT}"
    NGX_BUILD_DIR_BUILDROOT="${NGX_BUILD_DIR_BUILDROOT:-$DIR_BUILDROOT}"
    NGX_BUILD_DIR_PREFIX="${NGX_BUILD_DIR_PREFIX:-$DIR_PREFIX}"

    # Build options

    if [[ "$NGX_BUILD_NOPOOL" == 1 ]]; then
        build_name+=" nopool"
        NGX_BUILD_CC_OPT="$NGX_BUILD_CC_OPT -DNGX_WASM_NOPOOL -DNGX_DEBUG_MALLOC"
    fi

    if [[ -n "$NGX_BUILD_FSANITIZE" ]]; then
        build_name+=" san:$NGX_BUILD_FSANITIZE"
        NGX_BUILD_CC_OPT="$NGX_BUILD_CC_OPT -Wno-unused-command-line-argument -g -fsanitize=$NGX_BUILD_FSANITIZE -fno-omit-frame-pointer"
        NGX_BUILD_LD_OPT="$NGX_BUILD_LD_OPT -fsanitize=$NGX_BUILD_FSANITIZE -ldl -lm -lpthread -lrt"
        # clang 13: -fsanitize-ignorelist=$NGX_WASM_DIR/asan.ignore
    fi

    if [[ -n "$NGX_WASM_RUNTIME_LIB" ]]; then
        NGX_BUILD_LD_OPT="$NGX_BUILD_LD_OPT -Wl,-rpath,$NGX_WASM_RUNTIME_LIB"
    fi

    if [[ "$NGX_BUILD_CLANG_ANALYZER" == 1 ]]; then
        build_name+=" clang-analyzer"
        NGX_BUILD_CMD="scan-build -o $DIR_WORK/scans \
            --exclude $DIR_WORK \
            -analyze-headers \
            --force-analyze-debug-code \
            --html-title='$NGX - ngx_wasm_module [${build_name[@]}]' \
            --use-cc=clang \
            --status-bugs"
        NGX_BUILD_DEBUG=1
    fi

    if [[ "$NGX_BUILD_GCOV" == 1 ]]; then
        build_name+=" gcov"
        NGX_BUILD_CC_OPT="$NGX_BUILD_CC_OPT --coverage"
        NGX_BUILD_LD_OPT="$NGX_BUILD_LD_OPT -fprofile-arcs"
    fi

    if [[ "$NGX_BUILD_DEBUG" == 1 ]]; then
        build_name+=" debug"
        build_opts+="--with-debug "
    fi

    if [[ -n "$NGX_WASM_RUNTIME" ]]; then
        build_name+=" $NGX_WASM_RUNTIME"
    fi

    if [[ "$NGX_BUILD_DYNAMIC_MODULE" == 1 ]]; then
        build_name+=" dyn"
        build_opts+="--add-dynamic-module=$NGX_WASM_DIR "
    else
        build_opts+="--add-module=$NGX_WASM_DIR "
    fi

    local name="${build_name[@]}"

    # Build options hash to determine rebuild

    local hash=$(echo "ngx=$ngx_ver.$ngx_src.\
                       build_name=$name.\
                       cc=$CC.\
                       conf_opt=$NGX_BUILD_CONFIGURE_OPT.\
                       cc_opt=$NGX_BUILD_CC_OPT.\
                       ld_opt=$NGX_BUILD_LD_OPT.\
                       dynamic=$NGX_BUILD_DYNAMIC_MODULE" | shasum | awk '{ print $1 }')

    if [[ ! -d "$NGX_BUILD_DIR_SRCROOT" \
          || ! -f "$NGX_BUILD_DIR_SRCROOT/.hash" \
          || $(cat "$NGX_BUILD_DIR_SRCROOT/.hash") != $(echo $hash) ]];
    then
        NGX_BUILD_FORCE=1

        rm -rf $NGX_BUILD_DIR_SRCROOT
        cp -R $ngx_src $NGX_BUILD_DIR_SRCROOT

        # Apply patches

        if [[ "$NGX_BUILD_NOPOOL" == 1 ]]; then
            get_no_pool_nginx

            set +e
            apply_patch -p1 "$DIR_NOPOOL/nginx-$ngx_ver-no_pool.patch" $NGX_BUILD_DIR_SRCROOT
            if ! [ $? -eq 0 ]; then
                notice "failed applying the no-pool patch, trying again"
                get_no_pool_nginx 1
                apply_patch -p1 "$DIR_NOPOOL/nginx-$ngx_ver-no_pool.patch" $NGX_BUILD_DIR_SRCROOT
                if ! [ $? -eq 0 ]; then
                    fatal "failed applying the no-pool patch"
                fi
            fi
            set -e
        fi
    fi

    # ngx_echo_module, ngx_headers_more_module do not support --without-http

    if ! [[ "$NGX_BUILD_CONFIGURE_OPT" =~ "--without-http" ]]; then
        build_opts+="--add-dynamic-module=$DIR_NGX_ECHO_MODULE "
        build_opts+="--add-dynamic-module=$DIR_NGX_HEADERS_MORE_MODULE "
    fi

    # Build

    pushd $NGX_BUILD_DIR_SRCROOT
        if [[ "$NGX_BUILD_FORCE" == 1 \
              || ! -f "Makefile" \
              || ! -d "$NGX_BUILD_DIR_BUILDROOT" \
              || "$NGX_WASM_DIR/config" -nt "Makefile" \
              || "$NGX_WASM_DIR/Makefile" -nt "Makefile" \
              || "$NGX_WASM_DIR/util/build.sh" -nt "Makefile" \
              || "$NGX_WASM_DIR/util/_lib.sh" -nt "Makefile" ]];
        then
            local configure="configure"

            if [[ -f "auto/configure" ]]; then
                configure="auto/configure"
            fi

            eval ./$configure \
                "--build='ngx_wasm_module [$name]'" \
                "--builddir=$NGX_BUILD_DIR_BUILDROOT" \
                "--prefix=$NGX_BUILD_DIR_PREFIX" \
                "--with-cc-opt='$NGX_BUILD_CC_OPT'" \
                "--with-ld-opt='$NGX_BUILD_LD_OPT'" \
                "--with-poll_module" \
                "${build_opts[@]}" \
                "$NGX_BUILD_CONFIGURE_OPT" \

            echo $hash > "$NGX_BUILD_DIR_SRCROOT/.hash"
        fi

        eval "$NGX_BUILD_CMD make -j${n_jobs}"
    popd
}

download() {
    local output=$1
    local url=$2

    if [ -f $output ]; then
        return
    fi

    mkdir -p $(dirname $output)

    if [ -x "$(command -v axel)" ]; then
        (axel -o $output $url && return 0) || (rm -f $output; fatal "failed to download $url")
        return
    fi

    if [ -x "$(command -v wget)" ]; then
        wget -O $output $url || (rm -f $output; fatal "failed to download $url")
        return
    fi

    echo "curl --fail -L $url >$output || (rm -f $output; fatal \"failed to download $url\")"
    curl --fail -L $url >$output || (rm -f $output; fatal "failed to download $url")
}

apply_patch() {
    local pnum=$1
    local patch=$2
    local dir=$3

    local patch_name=$(basename -- "$patch")
    patch_name="${patch_name%.*}"

    echo "applying the $patch_name patch..."

    if [[ ! -z $dir ]]; then
        dir="-d $dir"
    fi

    patch $dir $pnum < $patch
}

get_no_pool_nginx() {
    if [[ -d "$DIR_NOPOOL" ]]; then
        if [[ ! -z "$1" ]]; then
            notice "updating the no-pool-nginx repository..."
            pushd $DIR_NOPOOL
                git fetch
                git reset --hard origin/master
            popd
        fi
    else
        notice "cloning the no-pool-nginx repository..."
        git clone https://github.com/openresty/no-pool-nginx.git $DIR_NOPOOL
    fi
}

download_wasmtime() {
    local wasmtime_ver=$1
    local arch=$(uname -m)
    case $OSTYPE in
        linux*)  os="linux" ;;
        darwin*) os="macos" ;;
        *)       os=$OSTYPE
    esac

    download wasmtime-$wasmtime_ver.tar.xz \
        "https://github.com/bytecodealliance/wasmtime/releases/download/v$wasmtime_ver/wasmtime-v$wasmtime_ver-$arch-$os-c-api.tar.xz"

    if [ ! -d "wasmtime-$wasmtime_ver" ]; then
        tar -xf wasmtime-$wasmtime_ver.tar.xz
        mv wasmtime-v$wasmtime_ver-$arch-$os-c-api wasmtime-$wasmtime_ver
    fi
}

download_wasmer() {
    local wasmer_ver=$1
    local arch=$2

    kernel=$(uname -s | tr '[:upper:]' '[:lower:]')

    download wasmer-$WASMER_VER.tar.gz \
        "https://github.com/wasmerio/wasmer/releases/download/$WASMER_VER/wasmer-$kernel-$arch.tar.gz"

    if [ ! -d "wasmer-$WASMER_VER" ]; then
        mkdir -p wasmer-${wasmer_ver}
        tar --directory=wasmer-$wasmer_ver -xf wasmer-$WASMER_VER.tar.gz
    fi
}

n_jobs() {
    local os=$(uname -s)

    if nproc 2>/dev/null >&2; then
        nproc

    elif [[ "$os" == "Darwin" ]]; then
        sysctl -n hw.physicalcpu

    else
        echo "1"
    fi
}

invalid_usage() {
    exec 1>&2

    if [ ! -z "$1" ]; then
        echo "Invalid usage: $1"
        echo
    fi

    show_usage

    exit 1
}

notice() {
    #builtin echo -en "\033[1;33m"
    #echo "NOTICE: $@"
    echo "$@"
    #builtin echo -en "\033[0m"
}

fatal() {
    exec 1>&2
    #builtin echo -en "\033[1;31m"
    echo "FATAL: $@"
    #builtin echo -en "\033[0m"
    exit 1
}

pushd() { builtin pushd $1 > /dev/null; }
popd() { builtin popd > /dev/null; }

# vim: ft=sh ts=4 sts=4 sw=4:
