DIR_WORK=$NGX_WASM_DIR/work
DIR_DOWNLOAD=$DIR_WORK/downloads
DIR_BUILDROOT=$DIR_WORK/buildroot
DIR_CPANM=$DIR_WORK/opt
DIR_BIN=$DIR_WORK/bin

build_nginx() {
    local ngx_dir=$1

    pushd $ngx_dir
        if [[ -n "$NGX_BUILD_FORCE" \
              || ! -f "Makefile" \
              || ! -d "$DIR_BUILDROOT" \
              || "$NGX_WASM_DIR/config" -nt "Makefile" \
              || "$NGX_WASM_DIR/Makefile" -nt "Makefile" \
              || "$NGX_WASM_DIR/util/build.sh" -nt "Makefile" \
              || "$NGX_WASM_DIR/util/_lib.sh" -nt "Makefile" ]];
        then
            ./configure \
                --build="ngx_wasm_module dev" \
                --builddir=$DIR_BUILDROOT \
                --add-module=$NGX_WASM_DIR \
                --with-cc-opt="-O0 -DNGX_WASM_USE_ASSERT" \
                --with-ld-opt="-Wl,-rpath,$WASMTIME_LIB" \
                --with-debug
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
        (axel -o $output $url && return 0) || rm -f $output
        return
    fi

    if [ -x "$(command -v wget)" ]; then
        fatal "missing required command 'wget'"
    fi

    wget -O $output $url || (rm -f $output; fatal "failed to download $url")
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
