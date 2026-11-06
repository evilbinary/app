/* test-gles-clear.c - Mesa EGL/GLES2 前端最小验证
 *
 * 目标：不走 gallium pipe API，直接：
 *   EGL device 平台 + GLES2 context + FBO + glClear + glReadPixels
 *
 * 运行前需要：
 *   /lib/dri/v3d_dri.so
 *   /lib/libEGL.so
 *   /lib/libGLESv2.so
 *   LIBGL_DRIVERS_PATH=/lib/dri
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#ifndef EGL_PLATFORM_DEVICE_EXT
#define EGL_PLATFORM_DEVICE_EXT 0x313F
#endif

static void dump_egl_error(const char* where) {
  EGLint e = eglGetError();
  printf("%s failed, egl error 0x%x\n", where, e);
}

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("=== test-gles-clear (EGL/GLES2) ===\n");

  PFNEGLQUERYDEVICESEXTPROC query_devices =
      (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
  PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display =
      (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress(
          "eglGetPlatformDisplayEXT");
  if (query_devices == NULL || get_platform_display == NULL) {
    printf("EGL device entrypoints missing\n");
    return 1;
  }

  EGLDeviceEXT devs[8];
  EGLint ndev = 0;
  if (!query_devices(8, devs, &ndev) || ndev < 1) {
    printf("eglQueryDevicesEXT failed, ndev=%d\n", ndev);
    dump_egl_error("eglQueryDevicesEXT");
    return 1;
  }
  printf("EGL devices: %d, using %p\n", ndev, (void*)devs[0]);

  EGLDisplay dpy = get_platform_display(EGL_PLATFORM_DEVICE_EXT,
                                        (void*)devs[0], NULL);
  printf("eglGetPlatformDisplay = %p\n", (void*)dpy);
  if (dpy == EGL_NO_DISPLAY) {
    dump_egl_error("eglGetPlatformDisplay");
    return 1;
  }

  EGLint major = 0, minor = 0;
  if (!eglInitialize(dpy, &major, &minor)) {
    dump_egl_error("eglInitialize");
    return 1;
  }
  printf("EGL %d.%d, vendor=%s\n", major, minor,
         eglQueryString(dpy, EGL_VENDOR));

  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    dump_egl_error("eglBindAPI");
    return 1;
  }

  EGLint cfg_attribs[] = {
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8, EGL_NONE,
  };
  EGLConfig cfg;
  EGLint ncfg = 0;
  if (!eglChooseConfig(dpy, cfg_attribs, &cfg, 1, &ncfg) || ncfg < 1) {
    dump_egl_error("eglChooseConfig");
    return 1;
  }

  EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attribs);
  printf("eglCreateContext = %p\n", (void*)ctx);
  if (ctx == EGL_NO_CONTEXT) {
    dump_egl_error("eglCreateContext");
    return 1;
  }

  if (!eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
    dump_egl_error("eglMakeCurrent");
    return 1;
  }
  printf("GL_VERSION=%s\n", glGetString(GL_VERSION));
  printf("GL_RENDERER=%s\n", glGetString(GL_RENDERER));

  /* FBO 640x480 RGBA8 */
  GLuint tex = 0, fbo = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 640, 480, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         tex, 0);
  GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  printf("FBO status=0x%x %s\n", st,
         st == GL_FRAMEBUFFER_COMPLETE ? "OK" : "FAIL");
  if (st != GL_FRAMEBUFFER_COMPLETE) return 1;

  glViewport(0, 0, 640, 480);
  glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  uint8_t px[4] = {0, 0, 0, 0};
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
  printf("pixel = %u %u %u %u (expect 0 255 0 255)\n", px[0], px[1], px[2],
         px[3]);
  int fail = !(px[0] == 0 && px[1] == 255 && px[2] == 0 && px[3] == 255);

  eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(dpy, ctx);
  eglTerminate(dpy);
  printf("=== %s ===\n", fail ? "FAIL" : "PASS");
  return fail;
}
