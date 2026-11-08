/* glxinfo - Wayland/EGL 版系统信息（原来的 glxinfo 依赖 GLX/X11）。
 *
 * 打印 EGL 客户端 API、device 平台、EGL/GL 版本、vendor/renderer、
 * 扩展和一部分 config，和 mesa-demos 的 glxinfo 输出对齐。
 */
#include <stdio.h>
#include <string.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#ifndef EGL_PLATFORM_DEVICE_EXT
#define EGL_PLATFORM_DEVICE_EXT 0x313F
#endif

static void print_exts(const char *name, const char *s) {
  printf("%s:\n", name);
  if (!s) {
    printf("  (null)\n");
    return;
  }
  while (*s) {
    const char *e = strchr(s, ' ');
    int n = e ? (int)(e - s) : (int)strlen(s);
    printf("  %.*s\n", n, s);
    if (!e) break;
    s = e + 1;
  }
}

int main(void) {
  PFNEGLQUERYDEVICESEXTPROC query_devices =
      (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
  PFNEGLGETPLATFORMDISPLAYEXTPROC get_display =
      (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress(
          "eglGetPlatformDisplayEXT");
  EGLDeviceEXT devs[8];
  EGLint ndev = 0, major = 0, minor = 0, ncfg = 0;
  EGLDisplay dpy;
  EGLConfig cfg;
  EGLContext ctx;
  EGLint cfg_attribs[] = {
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE,
  };
  EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

  if (!query_devices || !get_display) {
    printf("EGL device entrypoints missing\n");
    return 1;
  }
  if (!query_devices(8, devs, &ndev) || ndev < 1) {
    printf("eglQueryDevicesEXT: no device\n");
    return 1;
  }
  printf("EGL devices: %d\n", (int)ndev);
  dpy = get_display(EGL_PLATFORM_DEVICE_EXT, devs[0], NULL);
  if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &major, &minor)) {
    printf("eglInitialize failed 0x%x\n", eglGetError());
    return 1;
  }
  printf("EGL version: %d.%d\n", (int)major, (int)minor);
  printf("EGL vendor: %s\n", eglQueryString(dpy, EGL_VENDOR));
  printf("EGL version string: %s\n", eglQueryString(dpy, EGL_VERSION));
  printf("EGL client APIs: %s\n", eglQueryString(dpy, EGL_CLIENT_APIS));
  print_exts("EGL extensions", eglQueryString(dpy, EGL_EXTENSIONS));

  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    printf("eglBindAPI(ES) failed\n");
    return 1;
  }
  if (!eglChooseConfig(dpy, cfg_attribs, &cfg, 1, &ncfg) || ncfg < 1) {
    printf("eglChooseConfig: none\n");
    return 1;
  }
  printf("EGL configs (first): rgba=%d%d%d%d depth=%d\n",
         (int)8, (int)8, (int)8, (int)8, (int)16);
  ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attribs);
  if (ctx == EGL_NO_CONTEXT ||
      !eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
    printf("eglMakeCurrent failed 0x%x\n", eglGetError());
    return 1;
  }
  printf("GL version: %s\n", glGetString(GL_VERSION));
  printf("GL renderer: %s\n", glGetString(GL_RENDERER));
  printf("GL vendor: %s\n", glGetString(GL_VENDOR));
  printf("GLSL version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
  print_exts("GL extensions", (const char *)glGetString(GL_EXTENSIONS));

  eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(dpy, ctx);
  eglTerminate(dpy);
  return 0;
}
