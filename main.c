#include "drm_util.h"
#include "errors.h"
#include "vcap.h"

#include <assert.h>
#include <getopt.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef bool (*frame_notify_t)(buf_t, uint32_t);

static void capture_loop(frame_notify_t notify)
{
  bool const continuous = (frame_count == 0);
  size_t count = frame_count;
  while ((count-- > 0) || continuous)
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

        abort_errno("select");
      }

      if (0 == r)
        abort_msg("select timeout");

      buf_t frame;
      uint32_t index;
      if (!read_frame(&frame, &index))
        continue;

      if (!notify(frame, index))
        return;

      break;
    }
  }
}

static bool dump_frames(buf_t frame, uint32_t index)
{
  if (out_buf)
    fwrite(frame.start, frame.length, 1, stdout);

  fflush(stderr);
  fprintf(stderr, ".");
  fflush(stdout);

  return true;
}

static drm_device_t drm_dev;

// TODO: is this necessary?
static void drm_notify_vblank(
  int fd, unsigned int frame, unsigned int sec, unsigned int usec, void* data)
{
  fprintf(stderr, "DRM vblank\n");
}

static bool drm_frames(buf_t frame, uint32_t index)
{
  uint8_t* in = frame.start;
  uint8_t* out = drm_dev.drm_bufs[index].drm_buff;

  // TODO: This is not the correct layout. It's bad and I should feel bad.
  size_t const w = resolution.x;
  size_t const h = resolution.y;
  size_t const pad = XYLON_DRM_STRIDE - w;
  for (size_t y = 0; y < h; y++)
  {
    for (size_t x = 0; x < w; x++)
      *out++ = *in++;

    memset(out, 0, pad);
    out += pad;
  }
  size_t const out_len = drm_dev.drm_bufs[index].dumb_buff_length;
  size_t const written = out - drm_dev.drm_bufs[index].drm_buff;
  memset(out, 0, out_len - written);
  out += (out_len - written);

  assert((w * h) == (in - frame.start));

  return drm_set_plane(&drm_dev, index);
}

static void usage(FILE* fp, char const* arg0)
{
  fprintf(
    fp,
    "Usage: %s [options]\n\n"
    "Version 1.3\n"
    "Options:\n"
    "-h | --help          Print this message\n"
    "-d | --device        Video device name [%s]\n"
    "-o | --output        Outputs stream to stdout\n"
    "-v | --video         Outputs stream to HDMI\n"
    "-c | --count         Number of frames to grab (0 is continuous) [%i]\n"
    "-r | --resolution    Resulution [%ux%u]\n"
    "-f | --framerate     Frame rate [%zu]\n"
    "\n",
    arg0,
    dev_name,
    frame_count,
    resolution.x,
    resolution.y,
    frame_rate);
}

static char const* short_options = "d:hovc:r:f:";

static const struct option long_options[] = {
  {"device", required_argument, NULL, 'd'},
  {"help", no_argument, NULL, 'h'},
  {"output", no_argument, NULL, 'o'},
  {"video", no_argument, NULL, 'o'},
  {"count", required_argument, NULL, 'c'},
  {"resolution", required_argument, NULL, 'r'},
  {"framerate", required_argument, NULL, 'f'},
  {0, 0, 0, 0}};

int main(int argc, char** argv)
{
  bool drm = false;
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

      case 'v':
        drm = true;
        break;

      case 'c':
        errno = 0;
        frame_count = strtol(optarg, NULL, 0);
        if (errno)
          abort_errno(optarg);
        break;

      case 'r':
        sscanf(optarg, "%ux%u", &resolution.x, &resolution.y);
        break;

      case 'f':
        errno = 0;
        frame_rate = strtol(optarg, NULL, 0);
        if (errno)
          abort_errno(optarg);
        break;

      default:
        usage(stderr, argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  open_device();
  init_device();

  bool (*frame_notify)(buf_t, uint32_t) = dump_frames;

  if (drm)
  {
    drm_dev.width = resolution.x;
    drm_dev.height = resolution.y;
    drm_dev.module = "xylon-drm";
    drm_dev.format = V4L2_PIX_FMT_YUYV;

    drm_init(&drm_dev, 4);
    drm_set_plane_state(&drm_dev, drm_dev.prim_plane.plane_id, false);

    drmEventContext evctx;
    memset(&evctx, 0, sizeof(evctx));
    evctx.version = DRM_EVENT_CONTEXT_VERSION;
    evctx.vblank_handler = drm_notify_vblank;

    frame_notify = drm_frames;
  }

  start_capturing();
  capture_loop(frame_notify);
  stop_capturing();

  uninit_device();
  close_device();
  fprintf(stderr, "\n");

  return 0;
}
