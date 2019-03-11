NATIVE_TARGET := ./build/native/vcap
CROSS_TARGET := ./build/cross/vcap
ELFSIZE += $(CROSS_TARGET).size

CC ?= clang

# TODO: make portable, README
CROSS_CC_PATH ?= /opt/Xilinx/SDK/2015.4/gnu/arm/lin/bin/
CROSS_CC ?= $(CROSS_CC_PATH)arm-xilinx-linux-gnueabi-gcc
CROSS_SIZE ?= $(CROSS_CC_PATH)arm-xilinx-linux-gnueabi-size

CFLAGS := \
	-std=gnu11 -O3 -g -Wall -Wpedantic \
	-fno-omit-frame-pointer

INCLUDES ?=

CROSS_LIBS := -lm -lv4l2 -lv4lconvert
CROSS_LIBS_PATH ?= \
	-L/home/theodus/Documents/rdf0286-zc702-zvik-base-trd-2015-4/software/xsdk/lib \
	-L/opt/Xilinx/SDK/2015.4/gnu/arm/lin/arm-xilinx-linux-gnueabi/libc/lib

SRCS := $(wildcard *.c)
NATIVE_OBJS := $(patsubst %.c,./build/native/%.o,$(SRCS))
CROSS_OBJS := $(patsubst %.c,./build/cross/%.o,$(SRCS))

.PHONY: default all clean native cross

default: all

all: native cross

clean:
	-rm -rf ./build

native: $(NATIVE_TARGET)

./build/native/%.o: %.c
	-mkdir -p ./build/native
	$(CC) $(CFLAGS) $(INCLUDES) -o "$@" -c "$<"

$(NATIVE_TARGET): $(NATIVE_OBJS)
	$(CC) -o "$@" "$<"

cross: $(CROSS_TARGET) $(ELFSIZE)

./build/cross/%.o: %.c
	-mkdir -p ./build/cross
	$(CROSS_CC) $(CFLAGS) $(INCLUDES) -o "$@" -c "$<"

$(CROSS_TARGET): $(CROSS_OBJS)
	$(CROSS_CC) $(CROSS_LIBS_PATH) $(CROSS_LIBS) -o "$@" "$<"

$(ELFSIZE): $(CROSS_TARGET)
	$(CROSS_SIZE) "$<" | tee "$@"
