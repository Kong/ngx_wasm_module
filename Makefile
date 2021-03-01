NGX ?= 1.19.3
NGX_BUILD_CC_OPT ?= -O0 -ggdb3 -gdwarf
NGX_BUILD_LD_OPT ?=
NGX_BUILD_DEBUG ?= 1
NGX_BUILD_NOPOOL ?= 0
NGX_BUILD_FSANITIZE ?=
NGX_BUILD_CLANG_ANALYZER ?= 0
NGX_BUILD_GCOV ?= 0
NGX_BUILD_FORCE ?= 0

export NGX_WASM_RUNTIME ?=
export NGX_WASM_RUNTIME_INC ?= /usr/local/opt/include
export NGX_WASM_RUNTIME_LIB ?= /usr/local/opt/lib
export NGX_WASM_RUNTIME_LD_OPT ?=

.PHONY: build
build:
	@NGX_BUILD_CC_OPT="${NGX_BUILD_CC_OPT}" \
	 NGX_BUILD_LD_OPT="${NGX_BUILD_LD_OPT}" \
	 NGX_BUILD_DEBUG="${NGX_BUILD_DEBUG}" \
	 NGX_BUILD_NOPOOL="${NGX_BUILD_NOPOOL}" \
	 NGX_BUILD_FSANITIZE="${NGX_BUILD_FSANITIZE}" \
	 NGX_BUILD_CLANG_ANALYZER="${NGX_BUILD_CLANG_ANALYZER}" \
	 NGX_BUILD_GCOV="${NGX_BUILD_GCOV}" \
	 NGX_BUILD_FORCE="${NGX_BUILD_FORCE}" \
	   	util/build.sh ${NGX}

.PHONY: setup
setup:
	@util/setup_dev.sh

.PHONY: test
test:
	@util/test.sh t/0*

.PHONY: test-build
test-build:
	@util/test.sh --no-test-nginx t/10-build

.PHONY: style
style:
	@util/style.pl src/*
	@!(grep -R -E -n -- '#define\s+DDEBUG\s+1' src && echo "DDEBUG detected in sources") >&2
	@!(grep -R -E -n -- '---\s+ONLY' t && echo "--- ONLY block detected") >&2

.PHONY: reindex
reindex:
	@util/reindex.sh "t/*.t"
	@util/reindex.sh "t/**/*.t"
	@util/reindex.sh "t/**/**/*.t"

.PHONY: clean
clean:
	@util/clean.sh

.PHONY: cleanall
cleanall:
	@util/clean.sh --all
