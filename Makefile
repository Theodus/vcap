TARGET = vcap

CC ?= clang
CFLAGS = -O3 -g -std=gnu11 -Wall -Wextra -Wpedantic

INPUT_DEVICE ?= /dev/video2
RESOLUTION ?= 640x480
FRAMERATE ?= 30
FRAMES ?= 60

.PHONY : all clean out.yuv play

all : $(TARGET)

clean :
	-rm -f $(TARGET) *.o *.yuv

$(TARGET) : main.c
	-$(CC) $(CFLAGS) -o $(TARGET) main.c

out.yuv : $(TARGET)
	./vcap -d $(INPUT_DEVICE) -c $(FRAMES) -r $(RESOLUTION) -f $(FRAMERATE) -o > out.yuv

play : out.yuv
	ffplay -hide_banner -video_size $(RESOLUTION) -pixel_format yuyv422 -f rawvideo -framerate $(FRAMERATE) -i out.yuv
