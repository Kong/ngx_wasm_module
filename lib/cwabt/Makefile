.PHONY: build install clean

TARGET=/usr/local
LIBDIR=$(TARGET)/lib
INCDIR=$(TARGET)/include

build:
	cargo build

install:
	mkdir -p "$(INCDIR)"
	cp cwabt.h "$(INCDIR)"
	mkdir -p "$(LIBDIR)"
	cp target/*/libcwabt.a "$(LIBDIR)"

clean:
	cargo clean
