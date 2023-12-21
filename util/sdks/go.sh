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

download_go_sdk() {
    local version="$1"
    local clean="$2"

    if [[ "$clean" = "clean" ]]; then
        rm -rfv "$DIR_PROXY_WASM_GO_SDK"
    fi

    if [[ -d "$DIR_PROXY_WASM_GO_SDK" ]]; then
        notice "updating the proxy-wasm-go-sdk repository..."
        pushd $DIR_PROXY_WASM_GO_SDK
            git fetch --tags
            git reset --hard "v$version"
        popd

    else
        notice "cloning proxy-wasm-go-sdk repository, version ${version}..."
        git clone \
            -c advice.detachedHead=false --depth 1 \
            --branch "v$version" \
            https://github.com/tetratelabs/proxy-wasm-go-sdk.git \
            $DIR_PROXY_WASM_GO_SDK
    fi
}

build_go_sdk() {
    local version="$1"
    local clean="$2"

    if [[ ! -d "$DIR_PROXY_WASM_GO_SDK" ]]; then
        download_go_sdk $version

    else
        local cur_version=$(cd $DIR_PROXY_WASM_GO_SDK && git describe --tags)

        if [[ $cur_version != "v$version" ]]; then
            download_go_sdk $version
        fi
    fi

    local hasher=sha1sum
    if [[ ! -x "$(command -v $hasher)" ]]; then
        hasher=shasum
    fi

    local hash_src=$(find $DIR_PROXY_WASM_GO_SDK -type f \( -name '*.go' -or -name '*.mod' \) -exec $hasher {} \; \
                     | $hasher | awk '{ print $1 }')

    if [[ -d "$DIR_PATCHED_PROXY_WASM_GO_SDK" \
          && -f "$DIR_PATCHED_PROXY_WASM_GO_SDK/.hash" \
          && $(cat "$DIR_PATCHED_PROXY_WASM_GO_SDK/.hash") == $(echo $hash_src)
          && -z "$clean" ]] && \
       find $DIR_PATCHED_PROXY_WASM_GO_SDK -name '*.wasm' | grep -q .
    then
        notice "Go examples already built"
        return
    fi

    rm -rf $DIR_PATCHED_PROXY_WASM_GO_SDK
    cp -R $DIR_PROXY_WASM_GO_SDK $DIR_PATCHED_PROXY_WASM_GO_SDK
    echo $hash_src > "$DIR_PATCHED_PROXY_WASM_GO_SDK/.hash"

    pushd $DIR_PATCHED_PROXY_WASM_GO_SDK
        set +e
        patch --forward --ignore-whitespace examples/http_auth_random/main.go <<'EOF'
            @@ -21,7 +21,7 @@ import (
                    "github.com/tetratelabs/proxy-wasm-go-sdk/proxywasm/types"
             )

            -const clusterName = "httpbin"
            +const clusterName = "httpbin:2000"

             func main() {
                    proxywasm.SetVMContext(&vmContext{})
EOF
        patch --forward --ignore-whitespace examples/dispatch_call_on_tick/main.go <<'EOF'
            @@ -88,7 +88,7 @@ func (ctx *pluginContext) OnTick() {
              } else {
                headers = append(headers, [2]string{":path", "/fail"})
              }
            -	if _, err := proxywasm.DispatchHttpCall("web_service", headers, nil, nil, 5000, ctx.callBack); err != nil {
            +	if _, err := proxywasm.DispatchHttpCall("web_service:81", headers, nil, nil, 5000, ctx.callBack); err != nil {
                proxywasm.LogCriticalf("dispatch httpcall failed: %v", err)
              }
             }
EOF
        set -e

        notice "compiling Go examples..."

        make build.examples
    popd
}

install_go_sdk_examples() {
    pushd $DIR_PATCHED_PROXY_WASM_GO_SDK
        cd examples

        find . -name "*.wasm" | while read f; do
            # flatten module names, for example:
            # "./shared_queue/sender/main.wasm" to "go_shared_queue_sender.wasm"
            name=$(echo "$f" | sed 's,./\(.*\)/main.wasm,go_\1.wasm,;s,/,_,g')
            cp -av "$f" "$DIR_TESTS_LIB_WASM/$name"
        done
    popd
}

###############################################################################

mode="$1"
version="$2"
clean="$3"

if [ "$mode" = "download" ]; then
    download_go_sdk "$version" "$clean"

elif [ "$mode" = "build" ]; then
    build_go_sdk "$version" "$clean"

elif [ "$mode" = "install" ]; then
    install_go_sdk_examples

else
    fatal "Unknown mode."
fi
