TARGET ?= ../../work
CXX := $(TARGET)/libwee8/repos/v8/third_party/llvm-build/Release+Asserts/bin/clang
CXXFLAGS += -I $(TARGET)/libwee8/repos/v8/include -std=c++17 -O3

build: libv8bridge.a

libv8bridge.a: bridge.o
	ar rcs $(TARGET)/lib/libv8bridge.a bridge.o

clean:
	find . -name "*.o" -exec rm '{}' ';'
