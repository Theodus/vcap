native_target := ./build/native/vcap
cross_target := ./build/cross/vcap
elfsize += $(cross_target).size

CC ?= clang

CROSS_TOOLCHAIN_PREFIX ?= /opt/Xilinx/SDK/2015.4/gnu/arm/lin
CROSS_CC ?= $(CROSS_TOOLCHAIN_PREFIX)/bin/arm-xilinx-linux-gnueabi-gcc
CROSS_SIZE ?= $(CROSS_TOOLCHAIN_PREFIX)/bin/arm-xilinx-linux-gnueabi-size

# TODO: -Wextra
# CFLAGS := \
# 	-std=gnu11 -O2 -g -Wall -Wpedantic \
# 	-fno-omit-frame-pointer

CFLAGS := \
	-std=gnu11 -O2 -g \
	-fno-omit-frame-pointer

LDFLAGS := -ldrm

# TODO: make portable again, update README

INCLUDES ?= \
	-I/home/theodus/Documents/xsdk/include \
	-I/home/theodus/Documents/xsdk/projects/video_lib/include \
	-I/home/theodus/Documents/xsdk/projects/filter_lib/include \
	-I/home/theodus/Documents/xsdk/include/libdrm

CROSS_LDFLAGS := -lm -ldrm -lv4l2 -lv4lconvert
VENDOR_LDPATH ?= /home/theodus/Documents/xsdk/lib
CROSS_LDFLAGS_PATH ?= \
	-L$(VENDOR_LDPATH) \
	-L$(CROSS_TOOLCHAIN_PREFIX)/arm-xilinx-linux-gnueabi/libc/lib

srcs = $(wildcard *.c)
headers = $(wildcard *.h)
native_obj = $(patsubst %.c,./build/native/%.o,$(srcs))
cross_obj = $(patsubst %.c,./build/cross/%.o,$(srcs))

.PHONY: default all clean native cross

default: all

all: native cross

clean:
	-rm -rf ./build

native: $(native_target)

./build/native/%.o: %.c $(headers)
	@mkdir -p ./build/native
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ -c $<

$(native_target): $(native_obj)
	$(CC) $(LDFLAGS) -o $@ $^

cross: $(cross_target) $(elfsize)

./build/cross/%.o: %.c $(headers)
	@mkdir -p ./build/cross
	$(CROSS_CC) $(CFLAGS) $(INCLUDES) -o $@ -c $<

$(cross_target): $(cross_obj)
	$(CROSS_CC) $(CROSS_LDFLAGS_PATH) $(CROSS_LDFLAGS) -o $@ $^

$(elfsize): $(cross_target)
	$(CROSS_SIZE) $< | tee $@
