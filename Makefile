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

INPUT_DEVICE ?= /dev/video2
RESOLUTION ?= 640x480
FRAMERATE ?= 30
FRAMES ?= 60

.PHONY: default all clean playback

default: all

all: $(NATIVE_TARGET) $(CROSS_TARGET) $(ELFSIZE)

clean:
	-rm -rf ./build $(TARGET) $(CROSS_TARGET) $(ELFSIZE)

./build/native/%.o: %.c
	-mkdir -p ./build/native
	$(CC) $(CFLAGS) $(INCLUDES) -o "$@" -c "$<"

$(NATIVE_TARGET): $(NATIVE_OBJS)
	$(CC) -o "$@" "$<"

./build/cross/%.o: %.c
	-mkdir -p ./build/cross
	$(CROSS_CC) $(CFLAGS) $(INCLUDES) -o "$@" -c "$<"

$(CROSS_TARGET): $(CROSS_OBJS)
	$(CROSS_CC) $(CROSS_LIBS_PATH) $(CROSS_LIBS) -o "$@" "$<"

$(ELFSIZE): $(CROSS_TARGET)
	$(CROSS_SIZE) "$<" | tee "$@"

out.yuv: $(TARGET)
	./vcap \
		-d $(INPUT_DEVICE) \
		-c $(FRAMES) \
		-r $(RESOLUTION) \
		-f $(FRAMERATE) -o \
		> out.yuv

playback: out.yuv
	ffplay \
		-hide_banner \
		-video_size $(RESOLUTION) \
		-pixel_format yuyv422 \
		-f rawvideo \
		-framerate $(FRAMERATE) \
		-i out.yuv
