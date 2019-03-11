NATIVE_TARGET := ./build/native/vcap
CROSS_TARGET := ./build/cross/vcap
ELFSIZE += $(CROSS_TARGET).size

CC ?= clang

CROSS_TOOLCHAIN_PREFIX ?= /opt/Xilinx/SDK/2015.4/gnu/arm/lin
CROSS_CC ?= $(CROSS_TOOLCHAIN_PREFIX)/bin/arm-xilinx-linux-gnueabi-gcc
CROSS_SIZE ?= $(CROSS_TOOLCHAIN_PREFIX)/bin/arm-xilinx-linux-gnueabi-size

# TODO: -Wextra
CFLAGS := \
	-std=gnu11 -O2 -g -Wall -Wpedantic \
	-fno-omit-frame-pointer

INCLUDES ?=

CROSS_LIBS := -lm -lv4l2 -lv4lconvert
VENDOR_LIB_PATH ?= ./vendor/zc702_trd_2015_4/
V4L2_VENDOR_PATH ?=
CROSS_LIBS_PATH ?= \
	-L$(V4L2_VENDOR_PATH) \
	-L$(CROSS_TOOLCHAIN_PREFIX)/arm-xilinx-linux-gnueabi/libc/lib

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
