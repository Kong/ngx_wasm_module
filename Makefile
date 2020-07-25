NGX ?= 1.19.1
NGX_BUILD_CCOPT ?= -O0 -DNGX_WASM_USE_ASSERT
NGX_BUILD_DEBUG ?= 1
NGX_BUILD_NOPOOL ?= 0
NGX_BUILD_FORCE ?= 0

export WASMTIME_INC ?= /usr/local/opt/include
export WASMTIME_LIB ?= /usr/local/opt/lib

.PHONY: build
build:
	@NGX_BUILD_CCOPT="${NGX_BUILD_CCOPT}" \
	 NGX_BUILD_DEBUG="${NGX_BUILD_DEBUG}" \
	 NGX_BUILD_NOPOOL="${NGX_BUILD_NOPOOL}" \
	 NGX_BUILD_FORCE="${NGX_BUILD_FORCE}" \
	   	util/build.sh ${NGX}

.PHONY: setup
setup:
	@util/setup_dev.sh

.PHONY: test
test:
	@util/test.sh -r t/

.PHONY: style
style:
	@util/style.pl src/*

.PHONY: reindex
reindex:
	@util/reindex.sh t/*.t t/**/*.t

.PHONY: clean
clean:
	@util/clean.sh

.PHONY: cleanall
cleanall:
	@util/clean.sh --all
