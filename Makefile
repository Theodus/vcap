CC ?= clang
CFLAGS = -O3 -g -std=gnu11 -Wall -Wextra -Wpedantic
TARGET = vcap
INPUT_DEVICE ?= /dev/video0

.PHONY : all clean perf play

all : $(TARGET)

clean :
	-rm -f $(TARGET) *.o out perf.*

$(TARGET) : main.c
	-$(CC) $(CFLAGS) -o $(TARGET) main.c

out : $(TARGET)
	./vcap -d $(INPUT_DEVICE) -c 60 -o > out

perf : $(TARGET)
	perf record -g ./vcap -d $(INPUT_DEVICE) -c 240

play : out
	ffplay -video_size 1280x720 -pixel_format yuyv422 -f rawvideo -framerate 30 -i out
