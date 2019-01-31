CC ?= clang
CFLAGS = -O3 -g -std=gnu11 -Wall -Wextra -Wpedantic
TARGET = vcap
INPUT_DEVICE ?= /dev/video2
RESOLUTION ?= 640x480
FRAMERATE ?= 30

.PHONY : all clean out perf play

all : $(TARGET)

clean :
	-rm -f $(TARGET) *.o out perf.*

$(TARGET) : main.c
	-$(CC) $(CFLAGS) -o $(TARGET) main.c

out : $(TARGET)
	./vcap -d $(INPUT_DEVICE) -c 120 -r $(RESOLUTION) -f $(FRAMERATE) -o > out

perf : $(TARGET)
	perf record -g ./vcap -d $(INPUT_DEVICE) -c 240

play : out
	ffplay -hide_banner -video_size $(RESOLUTION) -pixel_format yuyv422 -f rawvideo -framerate $(FRAMERATE) -i out
