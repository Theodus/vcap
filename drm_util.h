#pragma once

#include "errors.h"

#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define MAX_DRM_BUFS 5
#define XYLON_DRM_STRIDE 2048

typedef struct
{
  unsigned int index;
  unsigned int bo_handle;
  unsigned int fb_handle;
  int dbuf_fd;
  unsigned char* drm_buff;
  unsigned int dumb_buff_length;
} drm_buf_t;

typedef enum
{
  PLANE_OVERLAY,
  PLANE_PRIMARY,
  PLANE_CURSOR,
  PLANE_NONE
} plane_t;

typedef struct
{
  char const* module;
  int fd;
  int crtc_index;
  unsigned int crtc_id;
  unsigned int con_id;
  drmModePlane prim_plane;
  drmModePlane overlay_plane;
  drmModeConnector* connector;
  drmModeModeInfo mode;
  drmModeCrtc* saved_crtc;
  const char modestr[32];
  unsigned int format;
  drm_buf_t drm_bufs[MAX_DRM_BUFS];
} drm_device_t;

// Create dumb scanout buffers and framebuffer
static inline bool drm_buffer_create(
  drm_device_t* dev, drm_buf_t* buf, size_t width, size_t height, size_t stride)
{
  struct drm_mode_create_dumb gem;
  struct drm_mode_map_dumb mreq;
  struct drm_mode_destroy_dumb gem_destroy;

  memset(&gem, 0, sizeof gem);
  gem.width = width;
  gem.height = height;
  gem.bpp = stride / width * 8;

  // The kernel will return a 32bit handle that can be used to manage the buffer
  // with the DRM API
  if (ioctl(dev->fd, DRM_IOCTL_MODE_CREATE_DUMB, &gem))
    return false;

  buf->bo_handle = gem.handle;
  buf->dumb_buff_length = gem.size;
  struct drm_prime_handle prime;
  memset(&prime, 0, sizeof prime);
  prime.handle = buf->bo_handle;

  if (ioctl(dev->fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime))
    goto fail_gem;

  buf->dbuf_fd = prime.fd;

  uint32_t offsets[4] = {0};
  uint32_t pitches[4] = {stride};
  uint32_t bo_handles[4] = {buf->bo_handle};

  int ret = drmModeAddFB2(
    dev->fd,
    width,
    height,
    dev->format,
    bo_handles,
    pitches,
    offsets,
    &buf->fb_handle,
    0);
  if (ret)
    goto fail_prime;

  memset(&mreq, 0, sizeof(mreq));
  mreq.handle = buf->bo_handle;
  if (drmIoctl(dev->fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq))
  {
    fprintf(stderr, "cannot map dumb buffer: %s\n", strerror(errno));
    goto fail_prime;
  }

  buf->drm_buff =
    mmap(0, gem.size, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, mreq.offset);
  if (buf->drm_buff == MAP_FAILED)
  {
    fprintf(stderr, "cannot mmap dumb buffer \n");
    goto fail_prime;
  }

  return true;

fail_prime:
  close(buf->dbuf_fd);

fail_gem:
  memset(&gem_destroy, 0, sizeof gem_destroy);
  gem_destroy.handle = buf->bo_handle,
  ioctl(dev->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &gem_destroy);
  return false;
}

// Find available CRTC and connector for scanout
static inline bool drm_find_crtc(drm_device_t* dev)
{
  drmModeRes* res = drmModeGetResources(dev->fd);
  if (res == NULL)
    return false;

  if ((res->count_crtcs <= 0) || (res->count_connectors <= 0))
  {
    drmModeFreeResources(res);
    return false;
  }

  dev->crtc_index = 0;
  dev->crtc_id = res->crtcs[0];

  dev->con_id = res->connectors[0];
  dev->connector = drmModeGetConnector(dev->fd, dev->con_id);
  if (!dev->connector)
  {
    drmModeFreeResources(res);
    return false;
  }

  return true;
}

static inline plane_t
drm_get_plane_type(drm_device_t* dev, unsigned int plane_id)
{
  plane_t type = PLANE_NONE;
  bool found = false;

  drmModeObjectPropertiesPtr props =
    drmModeObjectGetProperties(dev->fd, plane_id, DRM_MODE_OBJECT_PLANE);

  for (size_t i = 0; i < props->count_props && !found; i++)
  {
    const char* enum_name = NULL;

    drmModePropertyPtr prop = drmModeGetProperty(dev->fd, props->props[i]);

    if (0 == strcmp(prop->name, "type"))
    {
      for (size_t j = 0; j < prop->count_enums; j++)
      {
        if (prop->enums[j].value == props->prop_values[i])
        {
          enum_name = prop->enums[j].name;
          break;
        }
      }

      if (0 == strcmp(enum_name, "Primary"))
        type = PLANE_PRIMARY;
      else if (0 == strcmp(enum_name, "Overlay"))
        type = PLANE_OVERLAY;
      else if (0 == strcmp(enum_name, "Cursor"))
        type = PLANE_CURSOR;

      found = true;
    }

    drmModeFreeProperty(prop);
  }

  drmModeFreeObjectProperties(props);
  return type;
}

// Find an unused plane that supports the format
static inline bool drm_find_plane(drm_device_t* dev)
{
  drmModePlaneResPtr planes = drmModeGetPlaneResources(dev->fd);
  if (planes == NULL)
    return false;

  for (size_t i = 0; i < planes->count_planes; ++i)
  {
    drmModePlanePtr plane = drmModeGetPlane(dev->fd, planes->planes[i]);
    if (plane == 0)
      break;

    plane_t type = drm_get_plane_type(dev, plane->plane_id);
    if (type == PLANE_PRIMARY)
      dev->prim_plane = *plane;

    if (plane->crtc_id)
      continue;

    if (!(plane->possible_crtcs & (1 << dev->crtc_index)))
    {
      drmModeFreePlane(plane);
      continue;
    }

    size_t j = 0;
    for (j = 0; j < plane->count_formats; ++j)
    {
      if (plane->formats[j] == dev->format)
        break;
    }
    if (j == plane->count_formats)
    {
      drmModeFreePlane(plane);
      continue;
    }

    dev->overlay_plane = *plane;
    drmModeFreePlane(plane);

    break;
  }

  drmModeFreePlaneResources(planes);
  return true;
}

static bool drm_set_mode(drm_device_t* dev, size_t width, size_t height)
{
  dev->saved_crtc = drmModeGetCrtc(dev->fd, dev->crtc_id);
  if (!dev->saved_crtc)
    return false;

  drmModeConnector* connector = dev->connector;
  bool mode_found = false;
  size_t j = 0;
  for (j = 0; j < connector->count_modes; j++)
  {
    if (
      connector->modes[j].hdisplay == width &&
      connector->modes[j].vdisplay == height)
    {
      mode_found = 1;
      break;
    }
  }

  if (!mode_found)
  {
    fprintf(
      stderr, "Monitor does not support resolution %zux%zu\n", width, height);
    return false;
  }

  int ret = drmModeSetCrtc(
    dev->fd,
    dev->saved_crtc->crtc_id,
    dev->saved_crtc->buffer_id,
    dev->prim_plane.x,
    dev->prim_plane.y,
    &dev->con_id,
    1,
    &connector->modes[j]);
  if (ret < 0)
    return false;

  return true;
}

// Allocate framebuffers and map to userland
static inline void drm_init(
  drm_device_t* dev, unsigned int num_buffers, size_t width, size_t height)
{
  dev->fd = drmOpen(dev->module, NULL);
  if (dev->fd < 0)
    abort_info("drmOpen failed", dev->module);

  for (size_t i = 0; i < num_buffers; ++i)
  {
    if (!drm_buffer_create(
          dev, &dev->drm_bufs[i], width, height, XYLON_DRM_STRIDE))
      abort_msg("unable to create buffer");

    dev->drm_bufs[i].index = i;
  }

  fprintf(stderr, "DRM buffers ready\n");

  if (drmSetClientCap(dev->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1))
    abort_msg("universal plane not supported");

  if (!drm_find_crtc(dev))
    abort_msg("unable to find CRTC");

  if (!drm_find_plane(dev))
    abort_msg("unable to find compatible plane");

  if (!drm_set_mode(dev, width, height))
    abort_msg("unable set DRM configuration");
}
