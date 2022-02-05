#ifndef __KT_DEVICES_H__
#define __KT_DEVICES_H__

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <fcntl.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <gbm.h>

#include <epoxy/gl.h>
#include <epoxy/egl.h>


typedef struct
{
  int drm_fd;
  drmModeConnectorPtr connector_p;
  drmModeFBPtr default_fb_p;
  drmModeCrtcPtr crtc_p;

  struct gbm_device *gbmdevice;
  struct gbm_surface *gbmsurface;

  struct gbm_bo *previous_bo;
  uint32_t previous_fb;

  int default_fb_width;
  int default_fb_height;
} device_t;

typedef struct
{
  EGLDisplay display;
  EGLSurface surface;
  EGLContext context;

  GLuint program;
  GLuint texture_id;
  int width;
  int height;
} canvas_t;



static void create_drm_device(device_t *device)
{
  int fd = open("/dev/dri/card0", O_RDWR);
  assert(fd >= 0);

  drmModeResPtr res = drmModeGetResources(fd);
  assert(res);

  for (int i = 0; i < res->count_connectors; ++i) {
    device->connector_p = drmModeGetConnector(fd, res->connectors[i]);
    assert(device->connector_p);

    // find a connected connection
    if (device->connector_p->connection == DRM_MODE_CONNECTED){
      break;
    }
    drmFree(device->connector_p);
    device->connector_p = NULL;
  }

  assert(device->connector_p);

  drmModeEncoderPtr encoder = drmModeGetEncoder(fd, device->connector_p->encoder_id);
  assert(encoder);

  device->crtc_p = drmModeGetCrtc(fd, encoder->crtc_id);
  assert(device->crtc_p);

  // original fb used for terminal
  device->default_fb_p = drmModeGetFB(fd, device->crtc_p->buffer_id);
  assert(device->default_fb_p);

  device->drm_fd = fd;
  device->default_fb_width = device->default_fb_p->width;
  device->default_fb_height = device->default_fb_p->height;

  drmFree(encoder);
  drmFree(res);

}

static void create_gbm_device(device_t *device)
{
  device->gbmdevice = gbm_create_device(device->drm_fd);
  assert(device->gbmdevice != NULL);


  device->gbmsurface = gbm_surface_create(device->gbmdevice,
                                          device->default_fb_width,
                                          device->default_fb_height,
                                          GBM_BO_FORMAT_ARGB8888,
                                          GBM_BO_USE_LINEAR|GBM_BO_USE_SCANOUT|GBM_BO_USE_RENDERING);
  assert(device->gbmsurface);

}


static EGLConfig get_egl_config(canvas_t *canvas)
{
  EGLint egl_config_attribs[] = {
    EGL_BLUE_SIZE,        8,
    EGL_GREEN_SIZE,       8,
    EGL_RED_SIZE,         8,
    EGL_DEPTH_SIZE,       24,
    EGL_SURFACE_TYPE,     EGL_WINDOW_BIT,
    EGL_NONE,
  };

  EGLint num_configs = 0;
  assert(eglGetConfigs(canvas->display, NULL, 0, &num_configs) == EGL_TRUE);

  EGLConfig *configs = (EGLConfig *)malloc(num_configs * sizeof(EGLConfig));
  assert(eglChooseConfig(canvas->display, egl_config_attribs, configs, num_configs,
			 &num_configs) == EGL_TRUE);

  assert(num_configs);
  printf("num config %d\n", num_configs);

  /* find a config whose native visual ID is the desired GBM format. */
  for (int i = 0; i < num_configs; ++i){
    EGLint gbm_format;

    assert(eglGetConfigAttrib(canvas->display, configs[i], EGL_NATIVE_VISUAL_ID,
			      &gbm_format) == EGL_TRUE);

    printf("gbm format %x\n", gbm_format);

    if (gbm_format == GBM_FORMAT_ARGB8888){
      EGLConfig ret = configs[i];
      free(configs);
      return ret;
    }

  }

  /* failed to find a config with matching GBM format */
  abort();
}


void CreateRenderDevice(device_t *device)
{
  create_drm_device(device);
  create_gbm_device(device);
}


void CreateRenderContext(device_t *device, canvas_t *canvas)
{
  assert(epoxy_has_egl_extension(EGL_NO_DISPLAY, "EGL_MESA_platform_gbm"));

  canvas->display = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_MESA, device->gbmdevice, NULL);
  assert(canvas->display != EGL_NO_DISPLAY);

  EGLint major_version;
  EGLint minor_version;
  assert(eglInitialize(canvas->display, &major_version, &minor_version) == EGL_TRUE);

  assert(eglBindAPI(EGL_OPENGL_ES_API) == EGL_TRUE);

  printf("EGL major version: %d, minor version: %d\n", major_version, minor_version);

  EGLConfig config = get_egl_config(canvas);

  canvas->surface = eglCreatePlatformWindowSurfaceEXT(canvas->display, config, device->gbmsurface, NULL);
  assert(canvas->surface != EGL_NO_SURFACE);

  const EGLint context_attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE
  };

  canvas->context = eglCreateContext(canvas->display, config, EGL_NO_CONTEXT, context_attribs);
  assert(canvas->context != EGL_NO_CONTEXT);
  assert(eglMakeCurrent(canvas->display, canvas->surface, canvas->surface, canvas->context) == EGL_TRUE);

  canvas->width = device->default_fb_width;
  canvas->height = device->default_fb_height;

}

void SwapBuffer(device_t *device, canvas_t *canvas)
{
  eglSwapBuffers(canvas->display, canvas->surface);

  struct gbm_bo *bo = gbm_surface_lock_front_buffer(device->gbmsurface);
  assert(bo);

  uint32_t customize_fb;
  assert(!drmModeAddFB(device->drm_fd, gbm_bo_get_width(bo),
                       gbm_bo_get_height(bo), 24,
                       gbm_bo_get_bpp(bo),
                       gbm_bo_get_stride(bo),
                       gbm_bo_get_handle(bo).u32,
                       &customize_fb));

  // show my fb
  assert(!drmModeSetCrtc(device->drm_fd,device->crtc_p->crtc_id, customize_fb, 0, 0,
                         &device->connector_p->connector_id, 1, &device->crtc_p->mode));

  if (device->previous_bo) {
    drmModeRmFB(device->drm_fd, device->previous_fb);
    gbm_surface_release_buffer(device->gbmsurface, device->previous_bo);
  }

  device->previous_bo = bo;
  device->previous_fb = customize_fb;

}

void RestoreDefaultFramebuffer(device_t *device)
{
  // restore previous fb
  assert(!drmModeSetCrtc(device->drm_fd, device->crtc_p->crtc_id, device->default_fb_p->fb_id, 0, 0, &device->connector_p->connector_id, 1, &device->crtc_p->mode));

  if (device->previous_bo) {
    drmModeRmFB(device->drm_fd, device->previous_fb);
    gbm_surface_release_buffer(device->gbmsurface, device->previous_bo);

    device->previous_bo = NULL;
    device->previous_fb = 0;
  }
}

void OutputDisplay(device_t *device)
{
  struct gbm_bo *bo = gbm_surface_lock_front_buffer(device->gbmsurface);
  assert(bo);

  uint32_t customize_fb;
  assert(!drmModeAddFB(device->drm_fd, gbm_bo_get_width(bo),
                       gbm_bo_get_height(bo), 24,
                       gbm_bo_get_bpp(bo),
                       gbm_bo_get_stride(bo),
                       gbm_bo_get_handle(bo).u32,
                       &customize_fb));

  // show my fb
  assert(!drmModeSetCrtc(device->drm_fd,device->crtc_p->crtc_id, customize_fb, 0, 0,
                         &device->connector_p->connector_id, 1, &device->crtc_p->mode));

  // hold on a moment
  sleep(5);

  // restore previous fb
  assert(!drmModeSetCrtc(device->drm_fd, device->crtc_p->crtc_id, device->default_fb_p->fb_id, 0, 0, &device->connector_p->connector_id, 1, &device->crtc_p->mode));

  gbm_surface_release_buffer(device->gbmsurface, bo);

}

#endif
