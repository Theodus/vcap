#include <assert.h>
#include <errno.h>
#include <fcntl.h> // low-level i/o
#include <getopt.h>
#include <linux/videodev2.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// V4L2 Video capture to hardware driver

/*
- check `v4l2-ctl --list-formats-ext`, for camera details

TODO:
  - Interact with driver
*/

typedef struct
{
  void* start;
  size_t length;
} buf_t;

typedef struct
{
  size_t x;
  size_t y;
} resolution_t;

static char* dev_name;
static int fd = -1;
static buf_t* buffers;
static size_t n_buffers;
static int out_buf;
static int frame_count = 70;
static resolution_t resolution = {1280, 720};
static size_t frame_rate = 10;

static void errno_err(const char* s)
{
  fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
  exit(EXIT_FAILURE);
}

static void fatal_err(const char* s)
{
  fprintf(stderr, "%s\n", s);
  exit(EXIT_FAILURE);
}

static void device_err(const char* s)
{
  fprintf(stderr, dev_name, "%s %s\n", s);
  exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void* arg)
{
  int r;
  do
  {
    r = ioctl(fh, request, arg);
  } while (-1 == r && EINTR == errno);

  return r;
}

static void init_mmap()
{
  size_t const buf_count = 32;

  struct v4l2_requestbuffers req = {
    .count = buf_count,
    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .memory = V4L2_MEMORY_MMAP,
  };

  if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
  {
    if (EINVAL == errno)
      device_err("does not support memory mapping");
    else
      errno_err("VIDIOC_REQBUFS");
  }

  if (req.count < buf_count)
    device_err("has insufficient buffer memory");

  buffers = calloc(req.count, sizeof(buf_t));
  if (NULL == buffers)
    fatal_err("out of memory");

  for (n_buffers = 0; n_buffers < req.count; n_buffers++)
  {
    struct v4l2_buffer buf = {
      .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
      .memory = V4L2_MEMORY_MMAP,
      .index = n_buffers,
    };

    if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
      errno_err("VIDIOC_QUERYBUF");

    buffers[n_buffers].length = buf.length;
    buffers[n_buffers].start = mmap(
      NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

    if (MAP_FAILED == buffers[n_buffers].start)
      errno_err("mmap");
  }
}

static void open_device()
{
  struct stat st;
  if (-1 == stat(dev_name, &st))
    errno_err(dev_name);

  if (!S_ISCHR(st.st_mode))
    device_err("is not a char device");

  fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
  if (-1 == fd)
    errno_err("cannot open device");
}

static void close_device()
{
  if (-1 == close(fd))
    errno_err("close");

  fd = -1;
}

static void init_device()
{
  struct v4l2_capability cap;
  if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap))
  {
    if (EINVAL == errno)
      device_err("is not a V4L2 device");
    else
      errno_err("VIDIOC_QUERYCAP");
  }

  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    device_err("is not a video capture device");

  if (!(cap.capabilities & V4L2_CAP_STREAMING))
    device_err("is not a video streaming device");

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
    errno_err("VIDIOC_S_FMT");

  struct v4l2_streamparm stream_params = {
    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
  };
  if (-1 == xioctl(fd, VIDIOC_G_PARM, &stream_params))
    errno_err("VIDIOC_G_PARM");
  stream_params.parm.capture.timeperframe.numerator = 1;
  stream_params.parm.capture.timeperframe.denominator = frame_rate;
  if (-1 == xioctl(fd, VIDIOC_S_PARM, &stream_params))
    errno_err("VIDIOC_S_PARM");

  // "buggy driver paranoia"
  // size_t min = fmt.fmt.pix.width * 2;
  // if (fmt.fmt.pix.bytesperline < min)
  //   fmt.fmt.pix.bytesperline = min;
  // min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
  // if (fmt.fmt.pix.sizeimage < min)
  //   fmt.fmt.pix.sizeimage = min;

  init_mmap();
}

static void uninit_device()
{
  for (size_t i = 0; i < n_buffers; i++)
  {
    if (-1 == munmap(buffers[i].start, buffers[i].length))
      errno_err("munmap");
  }

  free(buffers);
}

static void start_capturing()
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  for (size_t i = 0; i < n_buffers; i++)
  {
    struct v4l2_buffer buf = {
      .type = type,
      .memory = V4L2_MEMORY_MMAP,
      .index = i,
    };
    if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
      errno_err("VIDIOC_QBUF");
  }

  if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
    errno_err("VIDIOC_STREAMON");
}

static void stop_capturing()
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
    errno_err("VIDIOC_STREAMOFF");
}

static void process_image(const void* p, int size)
{
  if (out_buf)
    fwrite(p, size, 1, stdout);

  fflush(stderr);
  fprintf(stderr, ".");
  fflush(stdout);
}

static bool read_frame()
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
        errno_err("VIDIOC_DQBUF");
    }
  }

  assert(buf.index < n_buffers);

  process_image(buffers[buf.index].start, buf.bytesused);

  if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
    errno_err("VIDIOC_QBUF");

  return true;
}

static void mainloop()
{
  size_t count = frame_count;
  while (count-- > 0)
  {
    while (true)
    {
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(fd, &fds);

      struct timeval tv = {.tv_sec = 2, .tv_usec = 0};
      int r = select(fd + 1, &fds, NULL, NULL, &tv);
      if (-1 == r)
      {
        if (EINTR == errno)
          continue;

        errno_err("select");
      }

      if (0 == r)
        fatal_err("select timeout");

      if (read_frame())
        break;

      // EAGAIN - continue select loop.
    }
  }
}

static void usage(FILE* fp, char const* arg0)
{
  fprintf(
    fp,
    "Usage: %s [options]\n\n"
    "Version 1.3\n"
    "Options:\n"
    "-h | --help          Print this message\n"
    "-d | --device name   Video device name [%s]\n"
    "-o | --output        Outputs stream to stdout\n"
    "-c | --count         Number of frames to grab [%i]\n"
    "-r | --resolution    Resulution [%zux%zu]\n"
    "-f | --framerate     Frame rate\n"
    "\n",
    arg0,
    dev_name,
    frame_count,
    resolution.x,
    resolution.y);
}

static char const* short_options = "d:hoc:r:f:";

static const struct option long_options[] = {
  {"device", required_argument, NULL, 'd'},
  {"help", no_argument, NULL, 'h'},
  {"output", no_argument, NULL, 'o'},
  {"count", required_argument, NULL, 'c'},
  {"resolution", required_argument, NULL, 'r'},
  {"framerate", required_argument, NULL, 'f'},
  {0, 0, 0, 0}};

int main(int argc, char** argv)
{
  dev_name = "/dev/video0";

  while (true)
  {
    int idx;
    int c = getopt_long(argc, argv, short_options, long_options, &idx);
    if (c == -1)
      break;

    switch (c)
    {
      case 0: // getopt_long() flag
        break;

      case 'd':
        dev_name = optarg;
        break;

      case 'h':
        usage(stdout, argv[0]);
        exit(EXIT_SUCCESS);

      case 'o':
        out_buf++;
        break;

      case 'c':
        errno = 0;
        frame_count = strtol(optarg, NULL, 0);
        if (errno)
          errno_err(optarg);
        break;

      case 'r':
        sscanf(optarg, "%zux%zu", &resolution.x, &resolution.y);
        break;

      case 'f':
        errno = 0;
        frame_rate = strtol(optarg, NULL, 0);
        if (errno)
          errno_err(optarg);
        break;

      default:
        usage(stderr, argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  open_device();
  init_device();
  start_capturing();
  mainloop();
  stop_capturing();
  uninit_device();
  close_device();
  fprintf(stderr, "\n");

  return 0;
}
