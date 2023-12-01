#!/usr/bin/env bash
set -e

SCRIPT_NAME=$(basename $0)
NGX_WASM_DIR=${NGX_WASM_DIR:-"$(
    cd $(dirname $(dirname $(dirname ${0})))
    pwd -P
)"}
if [[ ! -f "${NGX_WASM_DIR}/util/_lib.sh" ]]; then
    echo "Unable to source util/_lib.sh" >&2
    exit 1
fi

source $NGX_WASM_DIR/util/_lib.sh

###############################################################################

download_assemblyscript_sdk() {
    local version="$1"
    local clean="$2"

    if [[ "$clean" = "clean" ]]; then
        rm -rfv "$DIR_PROXY_WASM_ASSEMBLYSCRIPT_SDK"
    fi

    if [[ -d "$DIR_PROXY_WASM_ASSEMBLYSCRIPT_SDK" ]]; then
        notice "updating the proxy-wasm-assemblyscript-sdk repository..."
        pushd $DIR_PROXY_WASM_ASSEMBLYSCRIPT_SDK
            git fetch --tags
            git reset --hard "v$version"
        popd

    else
        notice "cloning proxy-wasm-assemblyscript-sdk repository, version ${version}..."
        git clone \
            -c advice.detachedHead=false --depth 1 \
            --branch "v$version" \
            https://github.com/Kong/proxy-wasm-assemblyscript-sdk.git \
            $DIR_PROXY_WASM_ASSEMBLYSCRIPT_SDK
    fi
}

build_assemblyscript_sdk() {
    local version="$1"
    local clean="$2"

    if [[ ! -x "$(command -v npm)" ]]; then
        fatal "missing 'npm', is Node.js installed?"
        exit 1
    fi

    if [[ ! -d "$DIR_PROXY_WASM_ASSEMBLYSCRIPT_SDK" ]]; then
        download_assemblyscript_sdk $version

    else
        local cur_version=$(cd $DIR_PROXY_WASM_ASSEMBLYSCRIPT_SDK && git describe --tags)

        if [[ $cur_version != "v$version" ]]; then
            download_assemblyscript_sdk $version
        fi
    fi

    local hasher=sha1sum
    if [[ ! -x "$(command -v $hasher)" ]]; then
        hasher=shasum
    fi

    local hash_src=$(find $DIR_PROXY_WASM_ASSEMBLYSCRIPT_SDK -type f \( -name '*.ts' -or -name 'package.json' \) -exec $hasher {} \; \
                     | $hasher | awk '{ print $1 }')

    if [[ -f "$DIR_PROXY_WASM_ASSEMBLYSCRIPT_SDK/.hash" \
          && $(cat "$DIR_PROXY_WASM_ASSEMBLYSCRIPT_SDK/.hash") == $(echo $hash_src)
          && -z $clean ]];
    then
        exit
    fi

    echo $hash_src > "$DIR_PROXY_WASM_ASSEMBLYSCRIPT_SDK/.hash"

    notice "compiling AssemblyScript examples..."

    for example in $DIR_PROXY_WASM_ASSEMBLYSCRIPT_SDK/examples/*; do
        name=$(basename $example)
        name=$(echo $name | sed 's/-/_/g')

        pushd $example
            npm install
            npm run asbuild
            cp build/debug.wasm $DIR_TESTS_LIB_WASM/assemblyscript_$name.wasm
        popd
    done
}

###############################################################################

mode="$1"
version="$2"
clean="$3"

if [ "$mode" = "download" ]; then
    download_assemblyscript_sdk "$version" "$clean"

else
    build_assemblyscript_sdk "$version" "$clean"
fi
