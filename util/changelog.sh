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

cd $NGX_WASM_DIR

# TODO: be smarter about grabbing the proper tag once we have more than just
# prerelease tags
LATEST_TAG=$(git describe --tags --abbrev=0)
OUT="$(git --no-pager log --format=oneline $LATEST_TAG..)"

if [[ -n $OUT ]]; then
    # Only show user-facing commits. Include refactor(), misc() and use best
    # judgement whether or not to include them in the final release
    # description.
    echo "$OUT" \
        | sed "s/^[^ ]* //" \
        | $(which grep) -v -E '^(chore|tests|style|hotfix|docs)'
fi
