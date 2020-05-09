NGX_VER ?= 1.17.10

export WASMTIME_INC ?= /usr/local/opt/include
export WASMTIME_LIB ?= /usr/local/opt/lib

.PHONY: build
build:
	@util/build.sh ${NGX_VER}

.PHONY: setup
setup:
	@util/setup_dev.sh

.PHONY: test
test:
	@util/test.sh

.PHONY: clean
clean:
	@util/clean.sh
