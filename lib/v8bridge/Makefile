.PHONY: build clean install libv8bridge

TARGET ?= /usr/local
V8_INCDIR ?= /usr/local/include
CXXFLAGS += -I $(V8_INCDIR) -std=c++17 -O3

build: libv8bridge

libv8bridge: bridge.o
	ar rcs libv8bridge.a bridge.o

install:
	cp ./libv8bridge.a $(TARGET)/lib/libv8bridge.a
	cp ./bridge.h $(TARGET)/include/v8bridge.h

clean:
	find . '(' -name "*.o" -o -name "*.a" ')' -a -exec rm '{}' ';'
