/* partial-draw-test.c - V3D 局部绘制/回读压力测试
 *
 * 目的：不依赖 weston/fbdev 后端，直接验证 V3D 的“只画脏区 + 只回读脏区”
 * 是否稳定。weston 的 fbdev GL 路径在拖窗口时就是：
 *   1) 对旧/新窗口矩形做 scissor 局部 draw；
 *   2) 只对 damage 矩形做 glReadPixels；
 *   3) 下一帧继续移动。
 *
 * 本程序用 1920x1080 RGBA8 FBO 循环移动一个 BOX 大小的小方块，
 * 每帧对旧/新两个矩形分别 scissor + draw + glReadPixels + glFinish。
 * 如果几十/几百帧后卡死或报 GPU 错误，就说明局部路径本身有问题。
 *
 * 运行：
 *   partial-draw-test           默认 5000 帧
 *   partial-draw-test 20000     指定帧数
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#ifndef EGL_PLATFORM_DEVICE_EXT
#define EGL_PLATFORM_DEVICE_EXT 0x313F
#endif

#define FB_W 1920
#define FB_H 1080
#define BOX 64

static const char* vs_src =
    "attribute vec2 a_pos;\n"
    "void main() { gl_Position = vec4(a_pos, 0.0, 1.0); }\n";

static const char* fs_src =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "void main() { gl_FragColor = u_color; }\n";

static GLuint compile(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, NULL);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetShaderInfoLog(s, sizeof(log), NULL, log);
    printf("shader compile failed: %s\n", log);
    return 0;
  }
  return s;
}

static GLuint make_program(void) {
  GLuint vs = compile(GL_VERTEX_SHADER, vs_src);
  GLuint fs = compile(GL_FRAGMENT_SHADER, fs_src);
  GLuint p;
  if (!vs || !fs) return 0;
  p = glCreateProgram();
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glBindAttribLocation(p, 0, "a_pos");
  glLinkProgram(p);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetProgramInfoLog(p, sizeof(log), NULL, log);
    printf("program link failed: %s\n", log);
    return 0;
  }
  return p;
}

/* 只画 x,y,w,h 这个矩形。quad 覆盖整个 NDC，scissor 决定真正落笔区域。 */
static void draw_rect(GLuint prog, int x, int y, int w, int h, float r,
                      float g, float b) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > FB_W) w = FB_W - x;
  if (y + h > FB_H) h = FB_H - y;
  if (w <= 0 || h <= 0) return;

  glEnable(GL_SCISSOR_TEST);
  glScissor(x, y, w, h);
  glUseProgram(prog);
  glUniform4f(glGetUniformLocation(prog, "u_color"), r, g, b, 1.0f);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDisable(GL_SCISSOR_TEST);
}

/* 只回读 x,y,w,h；每次回读后调用 glFinish，模拟 fbdev 后端的同步读回。 */
static void read_rect(int x, int y, int w, int h, uint8_t* buf, int frame) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > FB_W) w = FB_W - x;
  if (y + h > FB_H) h = FB_H - y;
  if (w <= 0 || h <= 0) return;

  glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf);
  glFinish();

  if ((frame % 100) == 0) {
    printf("frame %d read %d,%d %dx%d first=%02x%02x%02x%02x\n", frame, x, y,
           w, h, buf[0], buf[1], buf[2], buf[3]);
    fflush(stdout);
  }
}

int main(int argc, char** argv) {
  int iters = (argc > 1) ? atoi(argv[1]) : 5000;
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("=== partial-draw-test %dx%d box=%d iters=%d ===\n", FB_W, FB_H, BOX,
         iters);

  PFNEGLQUERYDEVICESEXTPROC query_devices =
      (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
  PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display =
      (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress(
          "eglGetPlatformDisplayEXT");
  if (!query_devices || !get_platform_display) {
    printf("EGL device entrypoints missing\n");
    return 1;
  }

  EGLDeviceEXT devs[8];
  EGLint ndev = 0;
  if (!query_devices(8, devs, &ndev) || ndev < 1) {
    printf("eglQueryDevicesEXT failed ndev=%d\n", ndev);
    return 1;
  }
  EGLDisplay dpy =
      get_platform_display(EGL_PLATFORM_DEVICE_EXT, (void*)devs[0], NULL);
  if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, NULL, NULL)) {
    printf("EGL init failed 0x%x\n", eglGetError());
    return 1;
  }
  eglBindAPI(EGL_OPENGL_ES_API);

  EGLint cfg_attribs[] = {
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8, EGL_NONE,
  };
  EGLConfig cfg;
  EGLint ncfg = 0;
  if (!eglChooseConfig(dpy, cfg_attribs, &cfg, 1, &ncfg) || ncfg < 1) {
    printf("eglChooseConfig failed 0x%x\n", eglGetError());
    return 1;
  }
  EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attribs);
  if (ctx == EGL_NO_CONTEXT ||
      !eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
    printf("EGL context failed 0x%x\n", eglGetError());
    return 1;
  }
  printf("GL_VERSION=%s\nGL_RENDERER=%s\n", glGetString(GL_VERSION),
         glGetString(GL_RENDERER));

  GLuint prog = make_program();
  if (!prog) return 1;

  GLuint tex = 0, fbo = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FB_W, FB_H, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         tex, 0);
  GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  printf("FBO status=0x%x %s\n", st,
         st == GL_FRAMEBUFFER_COMPLETE ? "OK" : "FAIL");
  if (st != GL_FRAMEBUFFER_COMPLETE) return 1;

  static const GLfloat verts[] = {
      -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f,
  };
  glViewport(0, 0, FB_W, FB_H);
  glUseProgram(prog);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);

  /* 先整屏清一次，之后只动小矩形。 */
  glDisable(GL_SCISSOR_TEST);
  glClearColor(0.05f, 0.05f, 0.10f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glFinish();

  static uint8_t buf[BOX * BOX * 4];
  int old_x = 0, old_y = 0;
  int i;

  for (i = 0; i < iters; i++) {
    int nx = (int)((i * 13) % (FB_W - BOX));
    int ny = (int)((i * 7) % (FB_H - BOX));

    /* 旧位置恢复背景（局部 draw） */
    draw_rect(prog, old_x, old_y, BOX, BOX, 0.05f, 0.05f, 0.10f);
    /* 新位置画方块（局部 draw） */
    draw_rect(prog, nx, ny, BOX, BOX, 1.0f, 0.0f, 0.0f);
    /* 只回读 damage 矩形 */
    read_rect(old_x, old_y, BOX, BOX, buf, i);
    read_rect(nx, ny, BOX, BOX, buf, i);

    old_x = nx;
    old_y = ny;
  }

  glFinish();
  printf("=== partial-draw-test PASS (%d frames) ===\n", iters);

  /* 正常释放 GPU 资源：gpud 的 BO 是全局表，客户端退出不主动销毁会占槽。 */
  glDeleteFramebuffers(1, &fbo);
  glDeleteTextures(1, &tex);
  glDeleteProgram(prog);
  eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(dpy, ctx);
  eglTerminate(dpy);
  return 0;
}
