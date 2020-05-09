NGX_VER ?= 1.17.10

.PHONY: default
default:
	@util/build.sh ${NGX_VER}

.PHONY: setup
setup:
	@util/setup_dev.sh

.PHONY: clean
clean:
	@util/clean.sh
