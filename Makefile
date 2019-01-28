CC ?= clang
CFLAGS = -g -std=gnu11 -Wall -Wextra -Wpedantic
TARGET = vcap

.PHONY : all clean play

all : $(TARGET)

clean :
	-rm -f $(TARGET) *.o

$(TARGET) : main.c
	-$(CC) $(CFLAGS) -o $(TARGET) main.c

out : $(TARGET)
	./vcap -d /dev/video2 -m -c 128 -o > out

play : out
	ffplay -video_size 1280x720 -pixel_format yuyv422 -f rawvideo -framerate 30 -i out
