/* fire - mesa-demos fire.c 的 Wayland/GLES2 重写。
 *
 * 原 demo 在 3D 里贴火焰纹理；这里用经典 "fire" 元胞算法在 CPU 上生成
 * 一张 128x128 火焰纹理，再用全屏 quad 拉伸上屏。效果对齐原 demo 的
 * 火焰观感，且不依赖任何固定管线。
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "demo.h"

#define FW 128
#define FH 128

static GLuint prog, tex, vbo;
static GLint loc_tex, loc_pos, loc_uv;
static uint8_t fire[FW * FH];
static uint8_t rgb[FW * FH * 3];
static uint8_t pal[256][3];
static unsigned int seed = 0x12345678;

static unsigned int rnd(void) {
  seed = seed * 1664525u + 1013904223u;
  return seed >> 16;
}

static void palette_init(void) {
  int i;
  for (i = 0; i < 256; i++) {
    int r, g, b;
    if (i < 64) {
      r = i * 3; g = 0; b = 0;
    } else if (i < 128) {
      r = 192 + (i - 64); g = (i - 64) * 2; b = 0;
    } else if (i < 192) {
      r = 255; g = 128 + (i - 128) * 2; b = 0;
    } else {
      r = 255; g = 255; b = (i - 192) * 4;
    }
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    pal[i][0] = (uint8_t)r;
    pal[i][1] = (uint8_t)g;
    pal[i][2] = (uint8_t)b;
  }
}

static void fire_step(void) {
  int x, y;
  /* 底部一行保持高温 */
  for (x = 0; x < FW; x++) {
    fire[x] = (uint8_t)(160 + rnd() % 96);
  }
  /* 从下往上传播并冷却 */
  for (y = 1; y < FH; y++) {
    for (x = 0; x < FW; x++) {
      int l = (x + FW - 1) % FW;
      int r = (x + 1) % FW;
      int v = fire[(y - 1) * FW + x] * 3 + fire[(y - 1) * FW + l] +
              fire[(y - 1) * FW + r] + fire[y * FW + x];
      v /= 6;
      v -= 2 + (int)(rnd() % 3);
      fire[y * FW + x] = (uint8_t)(v > 0 ? v : 0);
    }
  }
  for (x = 0; x < FW * FH; x++) {
    const uint8_t *p = pal[fire[x]];
    rgb[x * 3 + 0] = p[0];
    rgb[x * 3 + 1] = p[1];
    rgb[x * 3 + 2] = p[2];
  }
}

static int init(void) {
  static const char *vs =
      "attribute vec2 aPos;\n"
      "attribute vec2 aUV;\n"
      "varying vec2 vUV;\n"
      "void main() {\n"
      "  vUV = aUV;\n"
      "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
      "}\n";
  static const char *fs =
      "precision mediump float;\n"
      "varying vec2 vUV;\n"
      "uniform sampler2D uTex;\n"
      "void main() {\n"
      "  gl_FragColor = vec4(texture2D(uTex, vUV).rgb, 1.0);\n"
      "}\n";
  static const float quad[] = {
      -1, -1, 0, 0, 1, -1, 1, 0, 1, 1, 1, 1,
      -1, -1, 0, 0, 1, 1, 1, 1, -1, 1, 0, 1,
  };

  palette_init();
  fire_step();

  prog = demo_program(vs, fs);
  if (!prog) return -1;
  loc_tex = glGetUniformLocation(prog, "uTex");
  loc_pos = glGetAttribLocation(prog, "aPos");
  loc_uv = glGetAttribLocation(prog, "aUV");

  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, FW, FH, 0, GL_RGB,
               GL_UNSIGNED_BYTE, rgb);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
  glDisable(GL_DEPTH_TEST);
  return 0;
}

static void draw(float t) {
  (void)t;
  fire_step();
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, FW, FH, GL_RGB, GL_UNSIGNED_BYTE,
                  rgb);
  glUseProgram(prog);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glUniform1i(loc_tex, 0);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glEnableVertexAttribArray(loc_pos);
  glEnableVertexAttribArray(loc_uv);
  glVertexAttribPointer(loc_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)0);
  glVertexAttribPointer(loc_uv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

int main(void) { demo_set_title("Fire"); return demo_run(init, draw); }
