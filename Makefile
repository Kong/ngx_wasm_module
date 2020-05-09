NGX_VER ?= 1.17.10

.PHONY: default
default:
	@util/build.sh ${NGX_VER}

.PHONY: clean
clean:
	@util/clean.sh
