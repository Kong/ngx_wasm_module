DIR_WORK=$NGX_WASM_DIR/work
DIR_BIN=$DIR_WORK/bin
DIR_DOWNLOAD=$DIR_WORK/downloads
DIR_CPANM=$DIR_DOWNLOAD/cpanm
DIR_NOPOOL=$DIR_DOWNLOAD/no-pool-nginx
DIR_ECHO=$DIR_DOWNLOAD/echo-nginx-module
DIR_BUILDROOT=$DIR_WORK/buildroot
DIR_SRCROOT=$DIR_WORK/nginx-patched
DIR_TESTS_LIB_WASM=$DIR_WORK/lib/wasm
DIR_PREFIX=$NGX_WASM_DIR/t/servroot

build_nginx() {
    local ngx_src=$1
    local ngx_ver=$2
    local build_name=(dev)
    local build_with_debug=""

    NGX_BUILD_DIR_SRCROOT="${NGX_BUILD_DIR_SRCROOT:-$DIR_SRCROOT}"
    NGX_BUILD_DIR_BUILDROOT="${NGX_BUILD_DIR_BUILDROOT:-$DIR_BUILDROOT}"
    NGX_BUILD_DIR_PREFIX="${NGX_BUILD_DIR_PREFIX:-$DIR_PREFIX}"

    # Build options

    if [[ -n "$NGX_WASM_RUNTIME" ]]; then
        build_name+=" $NGX_WASM_RUNTIME"
    fi

    if [[ "$NGX_BUILD_NOPOOL" == 1 ]]; then
        build_name+=" nopool"
        NGX_BUILD_CC_OPT="$NGX_BUILD_CC_OPT -DNGX_WASM_NOPOOL -DNGX_DEBUG_MALLOC"
    fi

    if [[ -n "$NGX_BUILD_FSANITIZE" ]]; then
        build_name+=" san:$NGX_BUILD_FSANITIZE"
        NGX_BUILD_CC_OPT="$NGX_BUILD_CC_OPT -Wno-unused-command-line-argument -g -fsanitize=$NGX_BUILD_FSANITIZE -fno-omit-frame-pointer"
        NGX_BUILD_LD_OPT="$NGX_BUILD_LD_OPT -fsanitize=$NGX_BUILD_FSANITIZE -ldl -lm -lpthread -lrt"
    fi

    NGX_BUILD_LD_OPT="$NGX_BUILD_LD_OPT -Wl,-rpath,$NGX_WASM_RUNTIME_LIB"

    if [[ "$NGX_BUILD_CLANG_ANALYZER" == 1 ]]; then
        build_name+=" clang-analyzer"
        NGX_BUILD_CMD="scan-build -o $DIR_WORK/scans \
            --exclude $DIR_WORK \
            -analyze-headers \
            --force-analyze-debug-code \
            --html-title='$NGX - ngx_wasm_module [${build_name[@]}]' \
            --status-bugs"
            #--use-cc=$CC \
        NGX_BUILD_DEBUG=1
    fi

    if [[ "$NGX_BUILD_GCOV" == 1 ]]; then
        build_name+=" gcov"
        NGX_BUILD_CC_OPT="$NGX_BUILD_CC_OPT --coverage"
        NGX_BUILD_LD_OPT="$NGX_BUILD_LD_OPT -fprofile-arcs"
    fi

    if [[ "$NGX_BUILD_DEBUG" == 1 ]]; then
        build_name+=" debug"
        build_with_debug="--with-debug"
    fi

    local name="${build_name[@]}"

    # Build options hash to determine rebuild

    local hash=$(echo "$CC.$ngx_ver.$ngx_src.$name.$NGX_BUILD_CC_OPT.$NGX_BUILD_LD_OPT" | shasum | awk '{ print $1 }')

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

            apply_patch -p1 "$DIR_NOPOOL/nginx-$ngx_ver-no_pool.patch" $NGX_BUILD_DIR_SRCROOT
        fi
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
                "--add-module=$NGX_WASM_DIR" \
                "--add-dynamic-module=$DIR_ECHO" \
                "--with-cc-opt='$NGX_BUILD_CC_OPT'" \
                "--with-ld-opt='$NGX_BUILD_LD_OPT'" \
                $build_with_debug

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

    curl --fail -L $url -o $output || (rm -f $output; fatal "failed to download $url")
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
