NGX ?= 1.27.0
OPENSSL ?= 3.3.1
WASMTIME ?= 22.0.0
WASMER ?= 3.1.1
V8 ?= 12.0.267.17
PCRE ?= 8.45
ZLIB ?= 1.3.1
LUAROCKS ?= 3.11.1
PROXY_WASM_GO_SDK ?= 0.21.0
PROXY_WASM_ASSEMBLYSCRIPT_SDK ?= 0.0.6

# util/runtime.sh - no makefile target
NGX_BUILD_WASMER_RUSTFLAGS ?= -g -C opt-level=0 -C debuginfo=1
NGX_BUILD_WASMTIME_RUSTFLAGS ?= -g -C opt-level=0 -C debuginfo=1
NGX_BUILD_WASMTIME_PROFILE ?= debug

# Private - ngx_wasmx_module development
export NGX_BUILD_DIR_SCR ?=
export NGX_BUILD_DIR_PATCHED ?=
export NGX_BUILD_DIR_BUILDROOT ?=
export NGX_BUILD_DIR_PREFIX ?=
export NGX_BUILD_CONFIGURE_OPT ?=
export NGX_BUILD_DYNAMIC_MODULE ?= 0
export NGX_BUILD_CC_OPT ?= -O0 -ggdb3 -gdwarf
export NGX_BUILD_LD_OPT ?=
export NGX_BUILD_SSL ?= 1
export NGX_BUILD_SSL_STATIC ?= 0
export NGX_BUILD_DEBUG ?= 1
export NGX_BUILD_NOPOOL ?= 0
export NGX_BUILD_FSANITIZE ?=
export NGX_BUILD_OPENRESTY ?=
export NGX_BUILD_OPENSSL ?=
export NGX_BUILD_CLANG_ANALYZER ?= 0
export NGX_BUILD_GCOV ?= 0
export NGX_BUILD_FORCE ?= 0

# Public - config
export NGX_IPC ?= 0
export NGX_WASM_RUNTIME ?= wasmtime
export NGX_WASM_RUNTIME_INC ?=
export NGX_WASM_RUNTIME_LIB ?=
export NGX_WASM_RUNTIME_LD_OPT ?=
export NGX_WASM_RUNTIME_NO_RPATH ?= 0
export NGX_WASM_CARGO ?= 1
export NGX_WASM_CARGO_PROFILE ?= debug

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
	@util/style.sh $$(find src/ \( -name '*.h' -o -name '*.c' \))
	@!(grep -rIEn -- '#define\s+DDEBUG\s+1' src && echo "DDEBUG detected in sources") >&2
	@!(find t/ -name '*.t' -exec grep -IEn -- '---\s+ONLY' {} + && echo "--- ONLY detected in tests") >&2

.PHONY: reindex
reindex:
	@util/reindex.sh "t/*.t"
	@util/reindex.sh "t/**/*.t"
	@util/reindex.sh "t/**/**/*.t"
	@util/reindex.sh "t/**/**/**/*.t"

.PHONY: update
update:
	@cargo update --manifest-path lib/Cargo.toml
	@cargo update --manifest-path t/lib/Cargo.toml

.PHONY: todo
todo:
	@/bin/grep -rIFn -- 'TODO' src/ lib/ t/ util/

.PHONY: act-build
act-build:
	@docker build \
		-t wasmx-build-ubuntu \
		-f ./assets/release/Dockerfiles/Dockerfile.ubuntu-22.04 \
		./assets/release/Dockerfiles

.PHONY: act
act: act-build
	@act --reuse --pull=false

.PHONY: changelog
changelog:
	@util/changelog.sh

.PHONY: coverage
coverage: clean
	NGX_BUILD_GCOV=1 make
	TEST_NGINX_RANDOMIZE=1 make test
	rm -rf work/coverage_data
	mkdir work/coverage_data
	find work/buildroot -name '*.gcda' | xargs -I '{}' cp '{}' work/coverage_data
	find work/buildroot -name '*.gcno' | xargs -I '{}' cp '{}' work/coverage_data
	find work/coverage_data -name '*wasm_debug*' | xargs -I '{}' rm '{}'
	gcov -t -k -o work/coverage_data $$(find src/ -name '*.[ch]') | less -R

.PHONY: clean
clean:
	@util/clean.sh

.PHONY: cleanup
cleanup:
	@util/clean.sh --more

.PHONY: cleanall
cleanall:
	@util/clean.sh --all
