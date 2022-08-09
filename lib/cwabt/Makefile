.PHONY: build install clean

TARGET ?= /usr/local
LIBDIR = $(TARGET)/lib
INCDIR = $(TARGET)/include

build:
	cargo build --release

install:
	mkdir -p "$(INCDIR)"
	cp cwabt.h "$(INCDIR)"
	mkdir -p "$(LIBDIR)"
	cp target/release/libcwabt.a "$(LIBDIR)"

clean:
	cargo clean
