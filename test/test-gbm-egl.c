/* test-gbm-egl.c - GBM + EGL_PLATFORM_GBM 验证
 *
 * 流程：gbm_create_device -> gbm_surface_create(RGB565)
 *       -> eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA)
 *       -> eglCreateWindowSurface -> GLES clear -> eglSwapBuffers
 *       -> gbm_surface_lock_front_buffer -> libgpu 上屏
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>

#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "gpu_client.h"

#define W 640
#define H 480
#ifndef EGL_PLATFORM_GBM_MESA
#define EGL_PLATFORM_GBM_MESA 0x31D7
#endif

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  dup2(STDOUT_FILENO, STDERR_FILENO);
  printf("=== test-gbm-egl ===\n");

  setenv("GBM_DEBUG", "1", 1);
  setenv("GBM_BACKENDS_PATH", "/lib", 1);
  void* dh = dlopen("/lib/dri_gbm.so", RTLD_NOW);
  printf("dlopen dri_gbm=%p err=%s\n", dh, dh ? "" : dlerror());
  if (dh) printf("gbmint_get_backend=%p\n", dlsym(dh, "gbmint_get_backend"));

  const char* fd_path = "/dev/console";
  int fd = open(fd_path, O_RDWR);
  if (fd < 0) { fd_path = "/dev/tty"; fd = open(fd_path, O_RDWR); }
  if (fd < 0) { fd_path = "/dev/fb"; fd = open(fd_path, O_RDWR); }
  if (fd < 0) { printf("no device fd, errno=%d\n", errno); return 1; }
  struct stat st;
  int frc = fstat(fd, &st);
  printf("fd=%d path=%s fstat=%d mode=0%o chr=%d\n", fd, fd_path, frc,
         frc == 0 ? (unsigned)st.st_mode : 0,
         frc == 0 ? S_ISCHR(st.st_mode) : -1);
  extern char* loader_get_driver_for_fd(int fd);
  char* dn = loader_get_driver_for_fd(fd);
  printf("loader_get_driver_for_fd=%s\n", dn ? dn : "NULL");
  errno = 0;
  struct gbm_device* dev = gbm_create_device(fd);
  printf("gbm_device=%p errno=%d\n", (void*)dev, errno);
  if (dev == NULL) return 1;

  struct gbm_surface* surf =
      gbm_surface_create(dev, W, H, GBM_FORMAT_RGB565,
                         GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  printf("gbm_surface=%p\n", (void*)surf);
  if (surf == NULL) return 1;

  PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display =
      (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress(
          "eglGetPlatformDisplayEXT");
  if (get_platform_display == NULL) return 1;
  EGLDisplay dpy = get_platform_display(EGL_PLATFORM_GBM_MESA, dev, NULL);
  EGLint major = 0, minor = 0;
  if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &major, &minor)) {
    printf("eglInitialize failed 0x%x\n", eglGetError());
    return 1;
  }
  printf("EGL %d.%d vendor=%s\n", major, minor,
         eglQueryString(dpy, EGL_VENDOR));
  if (!eglBindAPI(EGL_OPENGL_ES_API)) return 1;

  /* 枚举所有 config，选与 GBM surface 格式（RGB565）完全匹配的那个。
   * eglChooseConfig 按“>= 请求值”匹配，5/6/5 会先给到 RGB101010，
   * 而 GBM surface 是 RGB565，eglCreateWindowSurface 会返回 EGL_BAD_MATCH。
   * DRM/GBM 平台用 EGL_NATIVE_VISUAL_ID 暴露对应的 GBM 格式。 */
  EGLConfig cfgs[64];
  EGLint ncfgs = 0;
  if (!eglGetConfigs(dpy, cfgs, 64, &ncfgs) || ncfgs <= 0) {
    printf("eglGetConfigs failed 0x%x\n", eglGetError());
    return 1;
  }
  printf("configs=%d\n", ncfgs);
  EGLConfig cfg = NULL;
  EGLint picked = -1;
  for (EGLint i = 0; i < ncfgs; i++) {
    EGLint vid = 0, rt = 0, st = 0;
    EGLint r = 0, g = 0, b = 0, a = 0;
    eglGetConfigAttrib(dpy, cfgs[i], EGL_NATIVE_VISUAL_ID, &vid);
    eglGetConfigAttrib(dpy, cfgs[i], EGL_RENDERABLE_TYPE, &rt);
    eglGetConfigAttrib(dpy, cfgs[i], EGL_SURFACE_TYPE, &st);
    eglGetConfigAttrib(dpy, cfgs[i], EGL_RED_SIZE, &r);
    eglGetConfigAttrib(dpy, cfgs[i], EGL_GREEN_SIZE, &g);
    eglGetConfigAttrib(dpy, cfgs[i], EGL_BLUE_SIZE, &b);
    eglGetConfigAttrib(dpy, cfgs[i], EGL_ALPHA_SIZE, &a);
    printf("cfg[%d] vid=0x%x rt=0x%x st=0x%x rgb=%d%d%d a=%d\n",
           i, vid, rt, st, r, g, b, a);
    if (cfg == NULL && vid == (EGLint)GBM_FORMAT_RGB565 &&
        (rt & EGL_OPENGL_ES2_BIT) && (st & EGL_WINDOW_BIT)) {
      cfg = cfgs[i];
      picked = i;
    }
  }
  if (cfg == NULL) {
    printf("no EGL config matches GBM_FORMAT_RGB565\n");
    return 1;
  }
  printf("picked cfg[%d]\n", picked);
  EGLint r = 0, g = 0, b = 0;
  eglGetConfigAttrib(dpy, cfg, EGL_RED_SIZE, &r);
  eglGetConfigAttrib(dpy, cfg, EGL_GREEN_SIZE, &g);
  eglGetConfigAttrib(dpy, cfg, EGL_BLUE_SIZE, &b);
  printf("config RGB=%d%d%d\n", r, g, b);

  EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attribs);
  EGLSurface esurf = eglCreateWindowSurface(dpy, cfg, surf, NULL);
  printf("egl ctx=%p surf=%p\n", (void*)ctx, (void*)esurf);
  if (ctx == EGL_NO_CONTEXT || esurf == EGL_NO_SURFACE) {
    printf("egl create failed 0x%x\n", eglGetError());
    return 1;
  }
  if (!eglMakeCurrent(dpy, esurf, esurf, ctx)) {
    printf("eglMakeCurrent failed 0x%x\n", eglGetError());
    return 1;
  }
  printf("GL_VERSION=%s GL_RENDERER=%s\n", glGetString(GL_VERSION),
         glGetString(GL_RENDERER));

  glViewport(0, 0, W, H);
  glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glFinish();
  if (!eglSwapBuffers(dpy, esurf)) {
    printf("eglSwapBuffers failed 0x%x\n", eglGetError());
    return 1;
  }

  struct gbm_bo* bo = gbm_surface_lock_front_buffer(surf);
  printf("gbm_bo=%p\n", (void*)bo);
  if (bo == NULL) return 1;
  union gbm_bo_handle h = gbm_bo_get_handle(bo);
  unsigned stride = gbm_bo_get_stride(bo);
  printf("bo handle=%u stride=%u\n", h.u32, stride);
  int rc = gpu_present_bo(h.u32, W, H, 1, stride, 0, 0, GPU_PRESENT_DIRECT);
  printf("gpu_present_bo rc=%d\n", rc);
  gbm_surface_release_buffer(surf, bo);

  int fail = (rc != 0);
  eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroySurface(dpy, esurf);
  eglDestroyContext(dpy, ctx);
  eglTerminate(dpy);
  gbm_surface_destroy(surf);
  gbm_device_destroy(dev);
  close(fd);
  printf("=== %s ===\n", fail ? "FAIL" : "PASS");
  return fail;
}
