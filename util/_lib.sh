DIR_WORK=$NGX_WASM_DIR/work
DIR_DOWNLOAD=$DIR_WORK/downloads
DIR_SRCROOT=$DIR_DOWNLOAD/nginx-patched
DIR_BUILDROOT=$DIR_WORK/buildroot
DIR_NOPOOL=$DIR_WORK/no-pool-nginx
DIR_CPANM=$DIR_WORK/opt
DIR_BIN=$DIR_WORK/bin
DIR_TESTS_LIB_WASM=t/lib/wasm

build_nginx() {
    local ngx_src=$1
    local ngx_ver=$2
    local build_name=(dev)
    local build_with_debug=""

    if [[ "$NGX_BUILD_NOPOOL" == 1 ]]; then
        build_name+=", no pool"
        NGX_BUILD_CCOPT="$NGX_BUILD_CCOPT -DNGX_WASM_NO_POOL"
    fi

    if [[ "$NGX_BUILD_DEBUG" == 1 ]]; then
        build_name+=", debug"
        build_with_debug="--with-debug"
    fi

    local hash=$(echo "$ngx_ver.$ngx_src.$build_name.$NGX_BUILD_CCOPT" | shasum | awk '{ print $1 }')

    if [[ ! -d "$DIR_SRCROOT" \
          || ! -f "$DIR_SRCROOT/.hash" \
          || $(cat "$DIR_SRCROOT/.hash") != $(echo $hash) ]];
    then
        NGX_BUILD_FORCE=1

        rm -rf $DIR_SRCROOT
        cp -R $ngx_src $DIR_SRCROOT

        if [[ "$NGX_BUILD_NOPOOL" == 1 ]]; then
            get_no_pool_nginx

            apply_patch -p1 "$DIR_NOPOOL/nginx-$ngx_ver-no_pool.patch" $DIR_SRCROOT
        fi
    fi

    pushd $DIR_SRCROOT
        if [[ "$NGX_BUILD_FORCE" == 1 \
              || ! -f "Makefile" \
              || ! -d "$DIR_BUILDROOT" \
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
                "--build='ngx_wasm_module ${build_name[@]}'" \
                "--builddir=$DIR_BUILDROOT" \
                "--add-module=$NGX_WASM_DIR" \
                "--with-cc-opt='$NGX_BUILD_CCOPT'" \
                "--with-ld-opt='-Wl,-rpath,$WASMTIME_LIB'" \
                $build_with_debug

            echo $hash > "$DIR_SRCROOT/.hash"
        fi

        make -j${n_jobs}
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

# vim: ft=sh st=4 sts=4 sw=4:
