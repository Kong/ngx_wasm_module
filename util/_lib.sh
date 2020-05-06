DIR_DOWNLOAD=$NGX_WASM_DIR/work
DIR_BUILDROOT=$NGX_WASM_DIR/buildroot

build_nginx() {
    local ngx_dir=$1

    pushd $ngx_dir
        if [[ -n "$NGX_BUILD_FORCE" || ! -f "Makefile" || \
              "$NGX_WASM_DIR/config" -nt "Makefile" ]]; then
            ./configure \
                --prefix=$DIR_BUILDROOT \
                --add-module=$NGX_WASM_DIR
        fi

        make -j${n_jobs}
        make install
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

fatal() {
    builtin echo -en "\033[1;31m"
    echo "FATAL: $@"
    builtin echo -en "\033[0m"
    exit 1
}

pushd() { builtin pushd $1 > /dev/null; }
popd() { builtin popd > /dev/null; }

# vim: ft=sh st=4 sts=4 sw=4:
