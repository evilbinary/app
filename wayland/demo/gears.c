/* gears - mesa-demos gears.c 的 Wayland/GLES2 重写。
 *
 * 原来的 gears 用 glBegin/glVertex 画齿轮、GLU 画轴；这里用 CPU 生成
 * 齿轮网格（齿、端面、侧壁）+ 简单 Blinn-Phong 着色器，三个齿轮按经典
 * 啮合关系反向旋转。
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "demo.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static GLuint prog, vbo;
static GLint loc_mvp, loc_model, loc_color, loc_pos, loc_nrm;
static int nverts;

struct gv {
  float p[3], n[3];
};

static void push(struct gv *v, int *n, float x, float y, float z, float nx,
                 float ny, float nz) {
  v[*n].p[0] = x; v[*n].p[1] = y; v[*n].p[2] = z;
  v[*n].n[0] = nx; v[*n].n[1] = ny; v[*n].n[2] = nz;
  (*n)++;
}

static int build_gear(int teeth, float r0, float r1, float thick) {
  int seg = teeth * 4;
  int maxv = seg * 24 + 64;
  struct gv *v = malloc((size_t)maxv * sizeof(*v));
  int n = 0, i;
  float hz = thick * 0.5f;
  if (!v) return -1;
  for (i = 0; i < seg; i++) {
    float a0 = 2.0f * (float)M_PI * i / seg;
    float a1 = 2.0f * (float)M_PI * (i + 1) / seg;
    int ph0 = i & 3, ph1 = (i + 1) & 3;
    float ra = (ph0 == 1 || ph0 == 2) ? r1 : r0;
    float rb = (ph1 == 1 || ph1 == 2) ? r1 : r0;
    float x0 = ra * cosf(a0), y0 = ra * sinf(a0);
    float x1 = rb * cosf(a1), y1 = rb * sinf(a1);
    /* front face (z=+hz), normal +z */
    push(v, &n, 0, 0, hz, 0, 0, 1);
    push(v, &n, x0, y0, hz, 0, 0, 1);
    push(v, &n, x1, y1, hz, 0, 0, 1);
    /* back face (z=-hz), normal -z */
    push(v, &n, 0, 0, -hz, 0, 0, -1);
    push(v, &n, x1, y1, -hz, 0, 0, -1);
    push(v, &n, x0, y0, -hz, 0, 0, -1);
    /* side wall */
    {
      float mx = (x0 + x1) * 0.5f, my = (y0 + y1) * 0.5f;
      float l = sqrtf(mx * mx + my * my);
      float nx = l > 1e-6f ? mx / l : 1.0f;
      float ny = l > 1e-6f ? my / l : 0.0f;
      push(v, &n, x0, y0, -hz, nx, ny, 0);
      push(v, &n, x1, y1, -hz, nx, ny, 0);
      push(v, &n, x1, y1, hz, nx, ny, 0);
      push(v, &n, x0, y0, -hz, nx, ny, 0);
      push(v, &n, x1, y1, hz, nx, ny, 0);
      push(v, &n, x0, y0, hz, nx, ny, 0);
    }
  }
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)n * sizeof(*v), v, GL_STATIC_DRAW);
  free(v);
  nverts = n;
  return 0;
}

static int init(void) {
  static const char *vs =
      "attribute vec4 aPosition;\n"
      "attribute vec3 aNormal;\n"
      "uniform mat4 uMVP;\n"
      "uniform mat4 uModel;\n"
      "varying vec3 vN;\n"
      "void main() {\n"
      "  vec4 p = uModel * aPosition;\n"
      "  vN = mat3(uModel) * aNormal;\n"
      "  gl_Position = uMVP * p;\n"
      "}\n";
  static const char *fs =
      "precision mediump float;\n"
      "varying vec3 vN;\n"
      "uniform vec3 uColor;\n"
      "void main() {\n"
      "  vec3 N = normalize(vN);\n"
      "  vec3 L = normalize(vec3(0.4, 0.6, 0.8));\n"
      "  float d = max(dot(N, L), 0.0);\n"
      "  vec3 c = uColor * (0.22 + 0.85 * d);\n"
      "  c += vec3(0.35) * pow(max(dot(reflect(-L, N), vec3(0,0,1)), 0.0), 20.0);\n"
      "  gl_FragColor = vec4(c, 1.0);\n"
      "}\n";
  prog = demo_program(vs, fs);
  if (!prog) return -1;
  loc_mvp = glGetUniformLocation(prog, "uMVP");
  loc_model = glGetUniformLocation(prog, "uModel");
  loc_color = glGetUniformLocation(prog, "uColor");
  loc_pos = glGetAttribLocation(prog, "aPosition");
  loc_nrm = glGetAttribLocation(prog, "aNormal");
  if (build_gear(20, 0.95f, 1.15f, 0.30f) != 0) return -1;
  glEnable(GL_CULL_FACE);
  return 0;
}

static void draw_gear(mat4 *pv, float x, float y, float angle, float scale,
                      float r, float g, float b) {
  mat4 model;
  mat4_identity(&model);
  mat4_translate(&model, x, y, 0.0f);
  mat4_rotate(&model, angle, 0, 0, 1);
  mat4_scale(&model, scale, scale, 1.0f);
  glUniformMatrix4fv(loc_mvp, 1, GL_FALSE, pv->m);
  glUniformMatrix4fv(loc_model, 1, GL_FALSE, model.m);
  glUniform3f(loc_color, r, g, b);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glEnableVertexAttribArray(loc_pos);
  glEnableVertexAttribArray(loc_nrm);
  glVertexAttribPointer(loc_pos, 3, GL_FLOAT, GL_FALSE, sizeof(struct gv),
                        (void *)0);
  glVertexAttribPointer(loc_nrm, 3, GL_FLOAT, GL_FALSE, sizeof(struct gv),
                        (void *)(3 * sizeof(float)));
  glDrawArrays(GL_TRIANGLES, 0, nverts);
}

static void draw(float t) {
  mat4 proj, view, pv, tilt;
  if (!prog) return;
  mat4_perspective(&proj, 45.0f, 640.0f / 480.0f, 0.5f, 100.0f);
  mat4_identity(&view);
  mat4_translate(&view, 0.0f, 0.0f, -7.0f);
  mat4_rotate(&tilt, 18.0f, 1.0f, 0.0f, 0.0f);
  mat4_mul(&view, &view, &tilt);
  mat4_mul(&pv, &proj, &view);

  glUseProgram(prog);
  draw_gear(&pv, -2.4f, 0.35f, t * 90.0f, 1.0f, 0.85f, 0.42f, 0.18f);
  draw_gear(&pv, 0.0f, 0.0f, -t * 57.0f, 1.35f, 0.30f, 0.65f, 0.85f);
  draw_gear(&pv, 2.5f, -0.30f, t * 75.0f, 0.85f, 0.55f, 0.85f, 0.35f);
}

int main(void) { demo_set_title("Gears"); return demo_run(init, draw); }
