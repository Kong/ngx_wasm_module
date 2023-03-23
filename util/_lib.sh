DIR_WORK=$NGX_WASM_DIR/work
DIR_BIN=$DIR_WORK/bin
DIR_DOWNLOAD=$DIR_WORK/downloads
DIR_CPANM=$DIR_DOWNLOAD/cpanm
DIR_NOPOOL=$DIR_DOWNLOAD/no-pool-nginx
DIR_NGX_ECHO_MODULE=$DIR_DOWNLOAD/echo-nginx-module
DIR_NGX_HEADERS_MORE_MODULE=$DIR_DOWNLOAD/headers-more-nginx-module
DIR_MOCKEAGAIN=$DIR_DOWNLOAD/mockeagain
DIR_PROXY_WASM_GO_SDK=$DIR_DOWNLOAD/proxy-wasm-go-sdk
DIR_BUILDROOT=$DIR_WORK/buildroot
DIR_SRC_ROOT=$DIR_WORK/nginx-src
DIR_PATCHED_ROOT=$DIR_WORK/nginx-patched
DIR_TESTS_LIB_WASM=$DIR_WORK/lib/wasm
DIR_DIST_WORK=$DIR_WORK/dist
DIR_DIST_RUNTIMES=$DIR_WORK/runtimes
DIR_LIBWEE8=$DIR_DIST_RUNTIMES/libwee8
DIR_PREFIX=$NGX_WASM_DIR/t/servroot
DIR_OPR_PREFIX=$DIR_BUILDROOT/prefix
DIR_DIST_OUT=$NGX_WASM_DIR/dist
DIR_LUAJIT=$DIR_OPR_PREFIX/luajit
DIR_LUAROCKS=$DIR_WORK/luarocks
BIN_LUAROCKS=$DIR_LUAROCKS/bin/luarocks
URL_KONG_WASM_RUNTIMES="https://github.com/kong/ngx_wasm_runtimes"

export PERL5LIB=$DIR_CPANM/lib/perl5

get_variable_from_makefile() {
    local var_name="$1"
    local makefile="$NGX_WASM_DIR/Makefile"

    awk '/'"$var_name"' \?= / { print $3 }' "$makefile"
}

# luarocks

LUAROCKS_VER=$(get_variable_from_makefile LUAROCKS)

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

# nginx

build_nginx() {
    local tree="$1"
    local build_opts=()
    local build_name=(dev)
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

    local ngx_ver=$(awk 'match($0, /NGINX_VERSION\s+"(.*?)"/, m) { print m[1] }' $header)

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

    if [[ "$NGX_BUILD_NOPOOL" == 1 ]]; then
        build_name+=" nopool"
        NGX_BUILD_CC_OPT="$NGX_BUILD_CC_OPT -DNGX_WASM_HAVE_NOPOOL -DNGX_DEBUG_MALLOC"
    fi

    if [[ -n "$NGX_BUILD_FSANITIZE" ]]; then
        build_name+=" san:$NGX_BUILD_FSANITIZE"
        NGX_BUILD_CC_OPT="$NGX_BUILD_CC_OPT -g \
                          -fsanitize=$NGX_BUILD_FSANITIZE \
                          -Wno-unused-command-line-argument \
                          -fno-omit-frame-pointer"
        NGX_BUILD_LD_OPT="$NGX_BUILD_LD_OPT \
                          -fsanitize=$NGX_BUILD_FSANITIZE \
                          -ldl -lm -lpthread -lrt"
        # clang 13: -fsanitize-ignorelist=$NGX_WASM_DIR/asan.ignore
    fi

    if [[ "$NGX_BUILD_CLANG_ANALYZER" == 1 ]]; then
        build_name+=" clang-analyzer"
        NGX_BUILD_CMD="scan-build -o $DIR_WORK/scans \
                       --exclude $DIR_WORK\
                       -analyze-headers \
                       --force-analyze-debug-code \
                       --html-title='$NGX - ngx_wasm_module [${build_name[@]}]' \
                       --use-cc=clang \
                       --status-bugs"
        NGX_BUILD_DEBUG=1
    fi

    if [[ "$NGX_BUILD_GCOV" == 1 ]]; then
        build_name+=" gcov"
        NGX_BUILD_CC_OPT="$NGX_BUILD_CC_OPT -fprofile-arcs -ftest-coverage"
        NGX_BUILD_LD_OPT="$NGX_BUILD_LD_OPT --coverage"
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

    if ! [[ "$NGX_BUILD_CONFIGURE_OPT" =~ "--without-http" ]]; then
        # ngx_echo_module, ngx_headers_more_module do not support --without-http
        # no need to add them when compiling OpenResty
        build_opts+="--with-http_v2_module " # tests with '--- http2' block
        build_opts+="--add-dynamic-module=$DIR_NGX_ECHO_MODULE "
        build_opts+="--add-dynamic-module=$DIR_NGX_HEADERS_MORE_MODULE "
    fi

    if [[ "$NGX_BUILD_SSL" == 1 ]]; then
        if ! [[ "$NGX_BUILD_CONFIGURE_OPT" =~ "--without-http" ]]; then
            build_opts+="--with-http_ssl_module "
        fi

        if [[ "$NGX_BUILD_CONFIGURE_OPT" =~ "--with-stream" ]]; then
            build_opts+="--with-stream_ssl_module "
            build_opts+="--with-stream_ssl_preread_module "
        fi
    fi

    local name="${build_name[@]}"

    #########
    # Patches
    #########

    # Source contents hash to determine repatch
    local hash_src=$(find $src_tree -type f \( -name '*.c' -or -name '*.h' \) -exec sha1sum {} \; \
                     | sha1sum | awk '{ print $1 }')

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
                        build_name=$name.\
                        cc=$CC.\
                        conf_opt=$NGX_BUILD_CONFIGURE_OPT.\
                        cc_opt=$NGX_BUILD_CC_OPT.\
                        ld_opt=$NGX_BUILD_LD_OPT.\
                        ssl=$NGX_BUILD_SSL.\
                        dynamic=$NGX_BUILD_DYNAMIC_MODULE.\
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

        # Wasm/Lua bridge test cases
        if [[ "$NGX_BUILD_DEBUG" == 1 ]]; then
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
