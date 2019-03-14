#pragma once

#include "errors.h"

#include <assert.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct
{
  void* start;
  size_t length;
} buf_t;

typedef struct
{
  uint32_t x;
  uint32_t y;
} resolution_t;

// TODO: proper config struct
static char* dev_name = "/dev/video7";
static int fd = -1;
static buf_t* buffers;
static uint32_t n_buffers;
static int out_buf;
static int frame_count = 120;
static resolution_t resolution = {640, 480};
static size_t frame_rate = 30;

static inline int xioctl(int fh, int request, void* arg)
{
  int r;
  do
  {
    r = ioctl(fh, request, arg);
  } while (-1 == r && EINTR == errno);

  return r;
}

static inline void init_mmap()
{
  size_t const buf_count = 4; // TODO: was 32

  struct v4l2_requestbuffers req = {
    .count = buf_count,
    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .memory = V4L2_MEMORY_MMAP,
  };

  if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
  {
    if (EINVAL == errno)
      abort_info(dev_name, "does not support memory mapping");
    else
      abort_errno("VIDIOC_REQBUFS");
  }

  if (req.count < buf_count)
    abort_info(dev_name, "has insufficient buffer memory");

  buffers = calloc(req.count, sizeof(buf_t));
  if (NULL == buffers)
    abort_msg("out of memory");

  for (n_buffers = 0; n_buffers < req.count; n_buffers++)
  {
    struct v4l2_buffer buf = {
      .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
      .memory = V4L2_MEMORY_MMAP,
      .index = n_buffers,
    };

    if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
      abort_errno("VIDIOC_QUERYBUF");

    buffers[n_buffers].length = buf.length;
    buffers[n_buffers].start = mmap(
      NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

    if (MAP_FAILED == buffers[n_buffers].start)
      abort_errno("mmap");
  }
}

static inline void open_device()
{
  struct stat st;
  if (-1 == stat(dev_name, &st))
    abort_errno(dev_name);

  if (!S_ISCHR(st.st_mode))
    abort_info(dev_name, "is not a char device");

  fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
  if (-1 == fd)
    abort_errno("cannot open device");
}

static inline void close_device()
{
  if (-1 == close(fd))
    abort_errno("close");

  fd = -1;
}

static inline void init_device()
{
  struct v4l2_capability cap;
  if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap))
  {
    if (EINVAL == errno)
      abort_info(dev_name, "is not a V4L2 device");
    else
      abort_errno("VIDIOC_QUERYCAP");
  }

  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    abort_info(dev_name, "is not a video capture device");

  if (!(cap.capabilities & V4L2_CAP_STREAMING))
    abort_info(dev_name, "is not a video streaming device");

  // select video input, video standard and tune

  struct v4l2_cropcap cropcap = {
    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
  };
  if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap))
  {
    struct v4l2_crop crop = {
      .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
      .c = cropcap.defrect, // reset to default
    };
    if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop))
    {
      switch (errno)
      {
        case EINVAL: // Cropping not supported
        default: // Errors ignored.
          break;
      }
    }
  }
  else
  {
    // TODO: Errors ignored.
  }

  struct v4l2_format fmt = {
    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .fmt.pix.width = resolution.x,
    .fmt.pix.height = resolution.y,
    .fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV,
    .fmt.pix.field = V4L2_FIELD_INTERLACED,
  };
  if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
    abort_errno("VIDIOC_S_FMT");

  struct v4l2_streamparm stream_params = {
    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
  };
  if (-1 == xioctl(fd, VIDIOC_G_PARM, &stream_params))
    abort_errno("VIDIOC_G_PARM");
  stream_params.parm.capture.timeperframe.numerator = 1;
  stream_params.parm.capture.timeperframe.denominator = frame_rate;
  if (-1 == xioctl(fd, VIDIOC_S_PARM, &stream_params))
    abort_errno("VIDIOC_S_PARM");

  // "buggy driver paranoia"
  // size_t min = fmt.fmt.pix.width * 2;
  // if (fmt.fmt.pix.bytesperline < min)
  //   fmt.fmt.pix.bytesperline = min;
  // min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
  // if (fmt.fmt.pix.sizeimage < min)
  //   fmt.fmt.pix.sizeimage = min;

  init_mmap();
}

static inline void uninit_device()
{
  for (size_t i = 0; i < n_buffers; i++)
  {
    if (-1 == munmap(buffers[i].start, buffers[i].length))
      abort_errno("munmap");
  }

  free(buffers);
}

static inline void start_capturing()
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  for (uint32_t i = 0; i < n_buffers; i++)
  {
    struct v4l2_buffer buf = {
      .type = type,
      .memory = V4L2_MEMORY_MMAP,
      .index = i,
    };
    if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
      abort_errno("VIDIOC_QBUF");
  }

  if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
    abort_errno("VIDIOC_STREAMON");
}

static inline void stop_capturing()
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
    abort_errno("VIDIOC_STREAMOFF");
}

static inline bool read_frame(buf_t* frame, uint32_t* index)
{
  struct v4l2_buffer buf = {
    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .memory = V4L2_MEMORY_MMAP,
  };
  if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
  {
    switch (errno)
    {
      case EAGAIN:
        return false;

      case EIO: // Could ignore EIO, see spec.
      default:
        abort_errno("VIDIOC_DQBUF");
    }
  }

  assert(buf.index < n_buffers);

  frame->start = buffers[buf.index].start;
  frame->length = buf.bytesused;
  *index = buf.index;

  if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
    abort_errno("VIDIOC_QBUF");

  return true;
}
