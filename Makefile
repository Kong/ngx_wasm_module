NGX ?= 1.19.1

export WASMTIME_INC ?= /usr/local/opt/include
export WASMTIME_LIB ?= /usr/local/opt/lib

.PHONY: build
build:
	@util/build.sh ${NGX}

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
