NGX ?= 1.19.10
export NGX_BUILD_CONFIGURE ?=
export NGX_BUILD_CC_OPT ?= -O0 -ggdb3 -gdwarf
export NGX_BUILD_LD_OPT ?=
export NGX_BUILD_DYNAMIC_MODULE ?= 0
export NGX_BUILD_DEBUG ?= 1
export NGX_BUILD_NOPOOL ?= 0
export NGX_BUILD_FSANITIZE ?=
export NGX_BUILD_CLANG_ANALYZER ?= 0
export NGX_BUILD_GCOV ?= 0
export NGX_BUILD_FORCE ?= 0

export NGX_WASM_RUNTIME ?= wasmtime
export NGX_WASM_RUNTIME_INC ?=
export NGX_WASM_RUNTIME_LIB ?=
export NGX_WASM_RUNTIME_LD_OPT ?=
export NGX_WASM_RUNTIME_PATH ?=

.PHONY: build
build:
	@util/build.sh ${NGX}

.PHONY: setup
setup:
	@util/setup_dev.sh

.PHONY: test
test:
	@util/test.sh t/0*

.PHONY: test-build
test-build:
	@util/test.sh --no-test-nginx t/10-build

.PHONY: lint
lint:
	@util/style.sh src/**/*.{h,c}
	@!(grep -REIn -- '#define\s+DDEBUG\s+1' src && echo "DDEBUG detected in sources") >&2
	@!(grep -REIn -- '---\s+ONLY' t/**/*.t && echo "--- ONLY detected in tests") >&2

.PHONY: reindex
reindex:
	@util/reindex.sh "t/*.t"
	@util/reindex.sh "t/**/*.t"
	@util/reindex.sh "t/**/**/*.t"

.PHONY: todo
todo:
	@/bin/grep -RIFn -- 'TODO' src/ t/

.PHONY: act
act:
	@docker build -t ubuntu-wasmx-dev ./util/ubuntu-wasmx-dev
	@act

.PHONY: clean
clean:
	@util/clean.sh

.PHONY: cleanup
cleanup:
	@util/clean.sh --all
