DIR_WORK=$NGX_WASM_DIR/work
DIR_BIN=$DIR_WORK/bin
DIR_SCANS=$DIR_WORK/scans
DIR_DOWNLOAD=$DIR_WORK/downloads
DIR_OPENSSL=$DIR_WORK/openssl
DIR_CPANM=$DIR_DOWNLOAD/cpanm
DIR_NOPOOL=$DIR_DOWNLOAD/no-pool-nginx
DIR_NGX_ECHO_MODULE=$DIR_DOWNLOAD/echo-nginx-module
DIR_NGX_HEADERS_MORE_MODULE=$DIR_DOWNLOAD/headers-more-nginx-module
DIR_MOCKEAGAIN=$DIR_DOWNLOAD/mockeagain
DIR_PROXY_WASM_ASSEMBLYSCRIPT_SDK=$DIR_DOWNLOAD/proxy-wasm-assemblyscript-sdk
DIR_PROXY_WASM_GO_SDK=$DIR_DOWNLOAD/proxy-wasm-go-sdk
DIR_PATCHED_PROXY_WASM_GO_SDK=$DIR_DOWNLOAD/proxy-wasm-go-sdk-patched
DIR_BUILDROOT=$DIR_WORK/buildroot
DIR_SRC_ROOT=$DIR_WORK/nginx-src
DIR_PATCHED_ROOT=$DIR_WORK/nginx-patched
DIR_TESTS_LIB_WASM=$DIR_WORK/lib/wasm
DIR_DIST_WORK=$DIR_WORK/dist
DIR_DIST_RUNTIMES=$DIR_WORK/runtimes
DIR_LIBWEE8=$DIR_DIST_RUNTIMES/libwee8
DIR_LIBWASMTIME=$DIR_DIST_RUNTIMES/libwasmtime
DIR_LIBWASMER=$DIR_DIST_RUNTIMES/libwasmer
DIR_PREFIX=$NGX_WASM_DIR/t/servroot
DIR_OPR_PREFIX=$DIR_BUILDROOT/prefix
DIR_DIST_OUT=$NGX_WASM_DIR/dist
DIR_LUAJIT=$DIR_OPR_PREFIX/luajit
DIR_LUAROCKS=$DIR_WORK/luarocks
BIN_LUAROCKS=$DIR_LUAROCKS/bin/luarocks
URL_KONG_WASM_RUNTIMES="https://github.com/kong/ngx_wasm_runtimes"

export PERL5LIB=$DIR_CPANM/lib/perl5

get_addon_name() {
    local config="$NGX_WASM_DIR/config"

    perl -nle"print if m{^ngx_addon_name=}" $config | cut -d "=" -f 2
}

get_variable_from_makefile() {
    local var_name="$1"
    local makefile="$NGX_WASM_DIR/Makefile"

    #grep -P "^$var_name \?=" $makefile | awk '{ print $3 }'
    # for macos compatibility:
    perl -nle"print if m{^$var_name \?=}" $makefile | awk '{ print $3 }'
}

NGX_VER=${NGX_VER:-$(get_variable_from_makefile NGX)}
OPENSSL_VER=${NGX_BUILD_OPENSSL:-$(get_variable_from_makefile OPENSSL)}
PCRE_VER=${PCRE_VER:-$(get_variable_from_makefile PCRE)}
ZLIB_VER=${ZLIB_VER:-$(get_variable_from_makefile ZLIB)}
LUAROCKS_VER=$(get_variable_from_makefile LUAROCKS)

# luarocks

install_luarocks() {
    notice "installing LuaRocks $LUAROCKS_VER..."

    if [[ ! -d "$DIR_DOWNLOAD/luarocks-$LUAROCKS_VER" ]]; then
        notice "downloading LuaRocks $LUAROCKS_VER..."
        download $DIR_DOWNLOAD/luarocks-$LUAROCKS_VER.tar.gz \
            "https://luarocks.org/releases/luarocks-$LUAROCKS_VER.tar.gz"
        tar -xf $DIR_DOWNLOAD/luarocks-$LUAROCKS_VER.tar.gz -C $DIR_DOWNLOAD
    fi

    pushd $DIR_DOWNLOAD/luarocks-$LUAROCKS_VER
        ./configure \
            --with-lua-include=$DIR_LUAJIT/include/luajit-2.1 \
            --with-lua-bin=$DIR_LUAJIT/bin \
            --prefix=$DIR_LUAROCKS
        make
        make install
    popd

    $BIN_LUAROCKS --tree=$DIR_LUAJIT --lua-version=5.1 install lua-resty-dns-client
    $BIN_LUAROCKS --tree=$DIR_LUAJIT --lua-version=5.1 install LuaFileSystem
}

remove_luarocks() {
    if [[ -x "$BIN_LUAROCKS" ]]; then
        local cur_ver=$($BIN_LUAROCKS --version | grep -Po '\d+\.\d+\.\d+')
        notice "removing LuaRocks $cur_ver..."
        $BIN_LUAROCKS --tree=$DIR_LUAJIT purge
        rm -rf $DIR_LUAROCKS
        rm -rf $DIR_LUAJIT/lib/luarocks
        rm -f $BIN_LUAROCKS
    fi
}

update_luarocks() {
    if [[ -x "$BIN_LUAROCKS" ]]; then
        local cur_ver=$($BIN_LUAROCKS --version | grep -Po '\d+\.\d+\.\d+')
        if [[ "$cur_ver" != "$LUAROCKS_VER" ]]; then
            # mostly for remove_luarocks, since updating the ver in the
            # Makefile triggered a fresh build
            remove_luarocks
        fi
    fi

    install_luarocks
}

# openssl

download_openssl() {
    mkdir -p $DIR_OPENSSL

    pushd $DIR_OPENSSL
        if [ ! -d "openssl-$OPENSSL_VER" ]; then
            pushd $DIR_DOWNLOAD
                notice "downloading OpenSSL $OPENSSL_VER..."
                download openssl-$OPENSSL_VER.tar.gz \
                    "https://www.openssl.org/source/openssl-$OPENSSL_VER.tar.gz"
            popd

            tar -xf $DIR_DOWNLOAD/openssl-$OPENSSL_VER.tar.gz
        fi
    popd
}

install_openssl() {
    download_openssl

    pushd $DIR_OPENSSL
        local dirname="openssl-lib-$OPENSSL_VER"

        if [ ! -d $dirname ]; then
            notice "building OpenSSL..."
            local prefix="$(pwd)/$dirname"
            pushd openssl-$OPENSSL_VER
                ./config \
                    --prefix=$prefix \
                    --openssldir=$prefix/openssl \
                    shared \
                    no-threads
                make -j$(n_jobs)
                make install_sw
            popd
        fi
    popd
}

# nginx

build_nginx() {
    local tree="$1"
    local build_opts=()
    local build_name="$(get_addon_name) dev"
    local build_name_opts=()
    local ngx_tree=$tree
    local src_tree=$tree

    if [[ -n "$NGX_BUILD_OPENRESTY" ]]; then
        src_tree="$tree/bundle"
        ngx_tree=$(find $src_tree -type d -name 'nginx-*')
    fi

    local header="$ngx_tree/src/core/nginx.h"
    if [[ ! -f "$header" ]]; then
        fatal "missing header file at $header, not an nginx source"
    fi

    # xargs: trim quotes
    local ngx_ver=$(grep -F "#define NGINX_VERSION" $header | awk '{ print $3 }' | xargs)

    if [[ -n "$NGX_WASM_RUNTIME" ]] && [[ -z "$NGX_WASM_RUNTIME_LIB" ]]; then
        local runtime_dir=$(get_default_runtime_dir "$NGX_WASM_RUNTIME")
        export NGX_WASM_RUNTIME_INC="$runtime_dir/include"
        export NGX_WASM_RUNTIME_LIB="$runtime_dir/lib"
    fi

    NGX_BUILD_DIR_SRC="${NGX_BUILD_DIR_SRC:-$DIR_SRC_ROOT}"
    NGX_BUILD_DIR_PATCHED="${NGX_BUILD_DIR_PATCHED:-$DIR_PATCHED_ROOT}"
    NGX_BUILD_DIR_BUILDROOT="${NGX_BUILD_DIR_BUILDROOT:-$DIR_BUILDROOT}"
    NGX_BUILD_DIR_PREFIX="${NGX_BUILD_DIR_PREFIX:-$DIR_PREFIX}"

    ###############
    # Build options
    ###############

    if [[ "$NGX_IPC" != 0 ]]; then
        build_name_opts+=("ipc")
    fi

    if [[ "$NGX_BUILD_NOPOOL" == 1 ]]; then
        build_name_opts+=("nopool")
        NGX_BUILD_CC_OPT="$NGX_BUILD_CC_OPT -DNGX_WASM_HAVE_NOPOOL -DNGX_DEBUG_MALLOC"
    fi

    if [[ -n "$NGX_BUILD_FSANITIZE" ]]; then
        build_name_opts+=("san:$NGX_BUILD_FSANITIZE")
        NGX_BUILD_CC_OPT="$NGX_BUILD_CC_OPT -g \
                          -fsanitize=$NGX_BUILD_FSANITIZE \
                          -fsanitize-blacklist=$NGX_WASM_DIR/asan.ignore \
                          -Wno-unused-command-line-argument \
                          -fno-omit-frame-pointer"
        NGX_BUILD_LD_OPT="$NGX_BUILD_LD_OPT \
                          -fsanitize=$NGX_BUILD_FSANITIZE \
                          -ldl -lm -lpthread -lrt"
        # clang 13: -fsanitize-ignorelist=$NGX_WASM_DIR/asan.ignore
    fi

    if [[ "$NGX_BUILD_GCOV" == 1 ]]; then
        build_name_opts+=("gcov")
        NGX_BUILD_CC_OPT="$NGX_BUILD_CC_OPT -fprofile-arcs -ftest-coverage"
        NGX_BUILD_LD_OPT="$NGX_BUILD_LD_OPT --coverage"
    fi

    if [[ "$NGX_BUILD_DEBUG" == 1 ]]; then
        build_name_opts+=("debug")
        build_opts+="--with-debug "
    fi

    if [[ -n "$NGX_WASM_RUNTIME" ]]; then
        build_name_opts+=("$NGX_WASM_RUNTIME")
    fi

    if [[ "$NGX_BUILD_DYNAMIC_MODULE" == 1 ]]; then
        build_name_opts+=("dyn")
        build_opts+="--add-dynamic-module=$NGX_WASM_DIR "

    else
        build_opts+="--add-module=$NGX_WASM_DIR "
    fi

    if ! [[ "$NGX_BUILD_CONFIGURE_OPT" =~ "--without-http" ]]; then
        # ngx_echo_module, ngx_headers_more_module do not support --without-http
        # no need to add them when compiling OpenResty
        build_opts+="--with-http_v2_module " # tests with '--- http2' block
        build_opts+="--add-dynamic-module=$DIR_NGX_ECHO_MODULE "
        build_opts+="--add-dynamic-module=$DIR_NGX_HEADERS_MORE_MODULE "
    fi

    if [[ "$NGX_BUILD_SSL" == 1 || "$NGX_BUILD_SSL_STATIC" == 1 ]]; then
        download_openssl

        if [[ "$NGX_BUILD_SSL_STATIC" == 1 ]]; then
            # native option which forces a full openssl build on fresh builds
            build_opts+="--with-openssl=$DIR_OPENSSL/openssl-$OPENSSL_VER "

        else
            local openssl_lib="$DIR_OPENSSL/openssl-lib-$OPENSSL_VER"
            local openssl_libdir=$(basename $openssl_lib/lib*)
            NGX_BUILD_CC_OPT="$NGX_BUILD_CC_OPT -I$openssl_lib/include"
            NGX_BUILD_LD_OPT="$NGX_BUILD_LD_OPT -L$openssl_lib/$openssl_libdir -Wl,-rpath,$openssl_lib/$openssl_libdir -lssl -lcrypto"
        fi

        if ! [[ "$NGX_BUILD_CONFIGURE_OPT" =~ "--without-http" ]]; then
            build_opts+="--with-http_ssl_module "
        fi

        if [[ "$NGX_BUILD_CONFIGURE_OPT" =~ "--with-stream" ]]; then
            build_opts+="--with-stream_ssl_module "
            build_opts+="--with-stream_ssl_preread_module "
        fi

        if [[ "$NGX_BUILD_CLANG_ANALYZER" == 1 ]]; then
            build_name_opts+=("clang-analyzer")
            NGX_BUILD_CMD="scan-build -o $DIR_SCANS \
                           --exclude $DIR_WORK\
                           -analyze-headers \
                           --force-analyze-debug-code \
                           --html-title='$NGX - $build_name [$(join ' ' ${build_name_opts[@]})]' \
                           --use-cc=${CC:-clang} \
                           --status-bugs"
            NGX_BUILD_DEBUG=1
        fi
    fi  # build options

    local name_opts=$(join ' ' ${build_name_opts[@]})

    #########
    # Patches
    #########

    local hasher=sha1sum
    if [[ ! -x "$(command -v $hasher)" ]]; then
        hasher=shasum
    fi

    # Source contents hash to determine repatch
    local hash_src=$(find $src_tree -type f \( -name '*.c' -or -name '*.h' \) -exec $hasher {} \; \
                     | $hasher | awk '{ print $1 }')

    if [[ ! -d "$NGX_BUILD_DIR_SRC" \
          || ! -f "$NGX_BUILD_DIR_SRC/.hash" \
          || $(cat "$NGX_BUILD_DIR_SRC/.hash") != $(echo $hash_src) ]];
    then
        rm -rf $NGX_BUILD_DIR_SRC
        cp -R $tree $NGX_BUILD_DIR_SRC
        echo $hash_src > "$NGX_BUILD_DIR_SRC/.hash"
    fi

    # Build options hash to determine rebuild
    local hash_opt_txt="ngx=$ngx_ver.$tree.\
                        build_opts=$name_opts.\
                        cc=$CC.\
                        conf_opt=$NGX_BUILD_CONFIGURE_OPT.\
                        cc_opt=$NGX_BUILD_CC_OPT.\
                        ld_opt=$NGX_BUILD_LD_OPT.\
                        ssl=$NGX_BUILD_SSL.$NGX_BUILD_SSL_STATIC.\
                        ipc=$NGX_IPC.\
                        dynamic=$NGX_BUILD_DYNAMIC_MODULE.\
                        cargo=$NGX_WASM_CARGO.\
                        hash_src=$hash_src"

    local hash_opt=$(echo $hash_opt_txt | shasum | awk '{ print $1 }')

    if [[ ! -d "$NGX_BUILD_DIR_PATCHED" \
          || ! -f "$NGX_BUILD_DIR_PATCHED/.hash" \
          || $(cat "$NGX_BUILD_DIR_PATCHED/.hash") != $(echo $hash_opt) ]];
    then
        rm -rf $NGX_BUILD_DIR_PATCHED
        cp -R $NGX_BUILD_DIR_SRC $NGX_BUILD_DIR_PATCHED
        echo $hash_opt > "$NGX_BUILD_DIR_PATCHED/.hash"

        NGX_BUILD_FORCE=1

        # Apply patches

        if [[ "$NGX_BUILD_NOPOOL" == 1 ]]; then
            if [[ -n "$NGX_BUILD_OPENRESTY" ]]; then
                build_opts+="--with-no-pool-patch "

            else
                get_no_pool_nginx

                set +e
                apply_patch -p1 "$DIR_NOPOOL/nginx-$ngx_ver-no_pool.patch" $NGX_BUILD_DIR_PATCHED
                if ! [ $? -eq 0 ]; then
                    notice "failed applying the no-pool patch, trying again"
                    get_no_pool_nginx 1
                    apply_patch -p1 "$DIR_NOPOOL/nginx-$ngx_ver-no_pool.patch" $NGX_BUILD_DIR_PATCHED
                    if ! [ $? -eq 0 ]; then
                        fatal "failed applying the no-pool patch"
                    fi
                fi
                set -e
            fi
        fi
    fi

    ###########
    # OpenResty
    ###########

    if [[ -n "$NGX_BUILD_OPENRESTY" ]]; then
        # switch prefix to preserve Lua components since t/servroot
        # is cleaned by Test::Nginx
        NGX_BUILD_DIR_PREFIX=$DIR_OPR_PREFIX

        if [[ "$NGX_BUILD_DEBUG" == 1 ]]; then
            # Wasm/Lua bridge test cases
            NGX_BUILD_CC_OPT="$NGX_BUILD_CC_OPT -DNGX_WASM_LUA_BRIDGE_TESTS"
        fi

        # ./configure -j for LuaJIT build
        build_opts+="-j$(n_jobs) "

        # built as dynamic modules above for Test::Wasm
        build_opts+="--without-http_echo_module "
        build_opts+="--without-http_headers_more_module "
    fi

    #######
    # Build
    #######

    pushd $NGX_BUILD_DIR_PATCHED
        if [[ "$NGX_BUILD_FORCE" == 1 \
              || ! -f "Makefile" \
              || ! -d "$NGX_BUILD_DIR_BUILDROOT" \
              || "$NGX_WASM_DIR/config" -nt "Makefile" \
              || "$NGX_WASM_DIR/auto/runtime" -nt "Makefile" \
              || "$NGX_WASM_DIR/auto/cargo" -nt "Makefile" \
              || "$NGX_WASM_DIR/Makefile" -nt "Makefile" \
              || "$NGX_WASM_DIR/util/build.sh" -nt "Makefile" \
              || "$NGX_WASM_DIR/util/_lib.sh" -nt "Makefile" \
              || "$NGX_WASM_DIR/asan.ignore" -nt "Makefile" \
              || "$NGX_WASM_DIR/lsan.suppress" -nt "Makefile" ]];
        then
            local configure="configure"

            if [[ -f "auto/configure" ]]; then
                configure="auto/configure"
            fi

            eval ./$configure \
                "--build='$build_name [$name_opts]'" \
                "--builddir=$NGX_BUILD_DIR_BUILDROOT" \
                "--prefix=$NGX_BUILD_DIR_PREFIX" \
                "--with-cc-opt='$NGX_BUILD_CC_OPT'" \
                "--with-ld-opt='$NGX_BUILD_LD_OPT'" \
                "--with-poll_module" \
                "${build_opts[@]}" \
                "$NGX_BUILD_CONFIGURE_OPT"

            FRESH_BUILD=1
        fi

        eval "$NGX_BUILD_CMD make -j$(n_jobs)"

        if [[ -n "$NGX_BUILD_OPENRESTY" || "$FRESH_BUILD" == 1 ]]; then
            make install
        fi

        if [[ -n "$NGX_BUILD_OPENRESTY" && "$FRESH_BUILD" == 1 ]]; then
            update_luarocks
        fi
    popd
}

# utils

join() {
    local sep=$1 ret=$2
    shift 2 || shift $(($#))
    printf "%s" "$ret${@/#/$sep}"
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

abs_path() {
    echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
}

get_default_runtime_version() {
    local runtime="$1"
    local var_name=$(echo "$runtime" | tr '[a-z]' '[A-Z]')

    get_variable_from_makefile "$var_name"
}

get_default_runtime_dir() {
    local runtime="$1"
    local version="${2:-$(get_default_runtime_version "$runtime")}"

    echo "$DIR_DIST_RUNTIMES/$runtime-$version"
}

get_default_proxy_wasm_sdk_version() {
    local lang=$(echo "$1" | tr '[a-z]' '[A-Z]')
    local var_name="PROXY_WASM_${lang}_SDK"

    get_variable_from_makefile "$var_name"
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
