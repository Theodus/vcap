#!/bin/sh

if [ "$#" -lt 1 ]; then
  echo "Usage: $0 input_YUYV422_file [frame_rate=30] [resolution=640x480] "
  echo " e.g.: $0 out.yuv 30 640x480"
  exit 1
fi

FRAMERATE="${2:-30}"
RESOLUTION="${3:-640x480}"

ffplay \
  -hide_banner \
  -video_size "$RESOLUTION" \
  -pixel_format yuyv422 \
  -f rawvideo \
  -framerate "$FRAMERATE" \
  -i out.yuv
