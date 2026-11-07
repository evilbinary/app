/* test-gles-texture.c - V3D 纹理路径最小验证
 *
 * 分阶段定位：
 *   1) FBO + glClear + glReadPixels（已知能跑）
 *   2) glTexImage2D 上传 CPU 纹理数据 + glFinish
 *   3) 全屏 quad 采样纹理 + glFinish + glReadPixels
 *
 * 运行前：gpud 已启动（EGL device 平台经它分配 BO）。
 * 任何阶段卡死，最后一行 stage 日志就是卡点。
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#ifndef EGL_PLATFORM_DEVICE_EXT
#define EGL_PLATFORM_DEVICE_EXT 0x313F
#endif

#define FB_W 128
#define FB_H 128
#define TEX_W 8
#define TEX_H 8

static GLuint compile_shader(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, NULL);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512];
    GLsizei n = 0;
    glGetShaderInfoLog(s, sizeof(log), &n, log);
    printf("shader compile fail: %.*s\n", (int)n, log);
    return 0;
  }
  return s;
}

static GLuint link_program(const char* vs_src, const char* fs_src) {
  GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
  GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
  if (!vs || !fs) return 0;
  GLuint p = glCreateProgram();
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glLinkProgram(p);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[512];
    GLsizei n = 0;
    glGetProgramInfoLog(p, sizeof(log), &n, log);
    printf("program link fail: %.*s\n", (int)n, log);
    return 0;
  }
  return p;
}

static void dump_px(const char* tag, GLuint fbo) {
  unsigned char px[4] = {0, 0, 0, 0};
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
  printf("%s: %u %u %u %u\n", tag, px[0], px[1], px[2], px[3]);
}

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("=== test-gles-texture ===\n");

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
    printf("eglQueryDevicesEXT failed ndev=%d err=0x%x\n", ndev, eglGetError());
    return 1;
  }

  EGLDisplay dpy = get_platform_display(EGL_PLATFORM_DEVICE_EXT, devs[0], NULL);
  if (dpy == EGL_NO_DISPLAY) {
    printf("eglGetPlatformDisplay failed err=0x%x\n", eglGetError());
    return 1;
  }
  EGLint major = 0, minor = 0;
  if (!eglInitialize(dpy, &major, &minor)) {
    printf("eglInitialize failed err=0x%x\n", eglGetError());
    return 1;
  }
  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    printf("eglBindAPI failed\n");
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
    printf("eglChooseConfig failed err=0x%x\n", eglGetError());
    return 1;
  }

  EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attribs);
  if (ctx == EGL_NO_CONTEXT) {
    printf("eglCreateContext failed err=0x%x\n", eglGetError());
    return 1;
  }
  if (!eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
    printf("eglMakeCurrent failed err=0x%x\n", eglGetError());
    return 1;
  }
  printf("GL_VERSION=%s GL_RENDERER=%s\n", glGetString(GL_VERSION),
         glGetString(GL_RENDERER));

  /* FBO 128x128 RGBA8 */
  GLuint fbo_tex = 0, fbo = 0;
  glGenTextures(1, &fbo_tex);
  glBindTexture(GL_TEXTURE_2D, fbo_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FB_W, FB_H, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         fbo_tex, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    printf("FBO incomplete\n");
    return 1;
  }

  /* stage 1: clear + read */
  printf("stage1: clear\n");
  glViewport(0, 0, FB_W, FB_H);
  glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glFinish();
  dump_px("stage1 pixel (expect 0 0 255 255)", fbo);

  /* stage 2: upload 8x8 checkerboard (red/green) */
  printf("stage2: texImage2D\n");
  uint8_t texdata[TEX_W * TEX_H * 4];
  for (int y = 0; y < TEX_H; y++) {
    for (int x = 0; x < TEX_W; x++) {
      uint8_t* p = &texdata[(y * TEX_W + x) * 4];
      if (((x ^ y) & 1) == 0) {
        p[0] = 255; p[1] = 0; p[2] = 0; p[3] = 255; /* red */
      } else {
        p[0] = 0; p[1] = 255; p[2] = 0; p[3] = 255; /* green */
      }
    }
  }
  GLuint src_tex = 0;
  glGenTextures(1, &src_tex);
  glBindTexture(GL_TEXTURE_2D, src_tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_W, TEX_H, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, texdata);
  glFinish();
  printf("stage2: upload done\n");

  /* stage 3: draw fullscreen quad with texture */
  static const char vs_src[] =
      "attribute vec2 aPos;\n"
      "attribute vec2 aUV;\n"
      "varying vec2 vUV;\n"
      "void main() {\n"
      "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
      "  vUV = aUV;\n"
      "}\n";
  static const char fs_src[] =
      "precision mediump float;\n"
      "varying vec2 vUV;\n"
      "uniform sampler2D uTex;\n"
      "void main() {\n"
      "  gl_FragColor = texture2D(uTex, vUV);\n"
      "}\n";
  GLuint prog = link_program(vs_src, fs_src);
  if (!prog) return 1;
  glUseProgram(prog);

  static const GLfloat verts[] = {
      -1.0f, -1.0f, 0.0f, 0.0f,
       1.0f, -1.0f, 1.0f, 0.0f,
      -1.0f,  1.0f, 0.0f, 1.0f,
       1.0f,  1.0f, 1.0f, 1.0f,
  };
  GLint aPos = glGetAttribLocation(prog, "aPos");
  GLint aUV = glGetAttribLocation(prog, "aUV");
  glVertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                        verts);
  glEnableVertexAttribArray(aPos);
  glVertexAttribPointer(aUV, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                        verts + 2);
  glEnableVertexAttribArray(aUV);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, src_tex);
  glUniform1i(glGetUniformLocation(prog, "uTex"), 0);

  printf("stage3: draw textured quad\n");
  glViewport(0, 0, FB_W, FB_H);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glFinish();
  printf("stage3: draw done\n");

  /* 左下角对应 texel (0,0) = red；右上角 texel (7,7) 也是 red */
  dump_px("stage3 bottom-left (expect red)", fbo);
  unsigned char px[4] = {0, 0, 0, 0};
  glReadPixels(FB_W - 2, FB_H - 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
  printf("stage3 top-right: %u %u %u %u (texel 7,7 red)\n", px[0], px[1],
         px[2], px[3]);

  /* stage4: 复现 weston 的 GL_BGRA_EXT / GL_BGRA8_EXT 上传 */
#define GL_BGRA_EXT 0x80E1
#define GL_BGRA8_EXT 0x93A1
  printf("stage4: texImage2D BGRA\n");
  GLuint tex_bgra = 0;
  glGenTextures(1, &tex_bgra);
  glBindTexture(GL_TEXTURE_2D, tex_bgra);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA8_EXT, TEX_W, TEX_H, 0,
               GL_BGRA_EXT, GL_UNSIGNED_BYTE, texdata);
  glFinish();
  printf("stage4: BGRA upload done\n");

  glBindTexture(GL_TEXTURE_2D, tex_bgra);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glFinish();
  printf("stage4: BGRA draw done\n");
  dump_px("stage4 bottom-left", fbo);

  /* stage5: GL_TEXTURE_SWIZZLE_RGBA（weston 对 shm 纹理也会设置） */
#define GL_TEXTURE_SWIZZLE_RGBA 0x8E46
#ifndef GL_RED
#define GL_RED 0x1903
#endif
#ifndef GL_GREEN
#define GL_GREEN 0x1904
#endif
#ifndef GL_BLUE
#define GL_BLUE 0x1905
#endif
  printf("stage5: swizzle RGBA\n");
  GLint swz[4] = {GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA};
  glBindTexture(GL_TEXTURE_2D, src_tex);
  glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swz);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glFinish();
  printf("stage5: swizzle draw done\n");
  dump_px("stage5 bottom-left", fbo);

  /* stage6: glDrawElements（weston 用的是 client-side index + DrawElements） */
  printf("stage6: drawElements\n");
  {
    static const GLushort idx[4] = {0, 1, 2, 3};

    glBindTexture(GL_TEXTURE_2D, src_tex);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_SHORT, idx);
    glFinish();
  }
  printf("stage6: drawElements done\n");
  dump_px("stage6 bottom-left", fbo);

  eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(dpy, ctx);
  eglTerminate(dpy);
  printf("=== test-gles-texture DONE ===\n");
  return 0;
}
