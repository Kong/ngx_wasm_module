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

if [[ ! -x "$(command -v tinygo)" ]]; then
    fatal "missing 'tinygo', is TinyGo installed?"
fi

PROXY_WASM_GO_SDK_TAG=${PROXY_WASM_GO_SDK_TAG:-v0.21.0}

if [[ -z "$1" ]]; then
    if ls $DIR_TESTS_LIB_WASM/go_* &> /dev/null; then
        exit 0
    fi

    if [[ -d "$DIR_PROXY_WASM_GO_SDK" ]]; then
        notice "updating the proxy-wasm-go-sdk repository..."
        pushd $DIR_PROXY_WASM_GO_SDK
            git fetch
            git reset --hard $PROXY_WASM_GO_SDK_TAG
        popd
    else
        notice "cloning the proxy-wasm-go-sdk repository..."
        git clone \
            -c advice.detachedHead=false --depth 1 \
            --branch $PROXY_WASM_GO_SDK_TAG \
            https://github.com/tetratelabs/proxy-wasm-go-sdk.git \
            $DIR_PROXY_WASM_GO_SDK
    fi
fi

pushd $DIR_PROXY_WASM_GO_SDK
    if [[ -z "$1" ]]; then
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
    fi

    notice "compiling Go examples..."

    if [ "$1" ]; then
        pushd examples/$1
            tinygo build -o main.wasm -scheduler=none -target=wasi ./main.go
        popd
    else
        make build.examples || exit 0
    fi

    cd examples

    find . -name "*.wasm" | while read f; do
        # flatten module names, for example:
        # "./shared_queue/sender/main.wasm" to "go_shared_queue_sender.wasm"
        name=$(echo "$f" | sed 's,./\(.*\)/main.wasm,go_\1.wasm,;s,/,_,g')
        cp -av "$f" "$DIR_TESTS_LIB_WASM/$name"
    done
popd
