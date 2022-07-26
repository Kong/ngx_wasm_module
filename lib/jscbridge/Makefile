.PHONY: build clean install libjscbridge

TARGET ?= /usr/local
JSC_INCDIR ?= /usr/local/include
CFLAGS += -I $(JSC_INCDIR) -O3

build: libjscbridge

libjscbridge: bridge.o
	ar rcs libjscbridge.a bridge.o

install:
	cp ./libjscbridge.a $(TARGET)/lib/libjscbridge.a
	cp ./bridge.h $(TARGET)/include/jscbridge.h
	cp ./wasm.h $(TARGET)/include/wasm.h

clean:
	find . '(' -name "*.o" -o -name "*.a" ')' -a -exec rm '{}' ';'
