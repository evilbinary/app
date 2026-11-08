/* fslight/fplight/arbfslight/arbfplight 的公共实现。
 *
 * 原 demo 都是 "一个球 + 一个方向光 + diffuse/specular"：
 *   fslight / arbfslight  在片元着色器里算光照
 *   fplight / arbfplight  在顶点程序里算光照（这里用顶点着色器等价实现）
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "demo.h"
#include "lighting_common.h"

static GLuint prog_frag, prog_vert, vbo_pos, vbo_nrm;
static GLint f_mvp, f_light, f_color, f_normal, f_pos;
static GLint v_mvp, v_light, v_color, v_normal, v_pos;
static int nverts;

static void make_meshes(void) {
  float *verts;
  int count, i;
  float *pos, *nrm;
  demo_sphere(2.0f, 32, 20, &verts, &count);
  nverts = count;
  pos = malloc((size_t)count * 3 * sizeof(float));
  nrm = malloc((size_t)count * 3 * sizeof(float));
  for (i = 0; i < count; i++) {
    pos[i * 3 + 0] = verts[i * 6 + 0];
    pos[i * 3 + 1] = verts[i * 6 + 1];
    pos[i * 3 + 2] = verts[i * 6 + 2];
    nrm[i * 3 + 0] = verts[i * 6 + 3];
    nrm[i * 3 + 1] = verts[i * 6 + 4];
    nrm[i * 3 + 2] = verts[i * 6 + 5];
  }
  free(verts);
  glGenBuffers(1, &vbo_pos);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_pos);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)count * 3 * sizeof(float), pos,
               GL_STATIC_DRAW);
  glGenBuffers(1, &vbo_nrm);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_nrm);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)count * 3 * sizeof(float), nrm,
               GL_STATIC_DRAW);
  free(pos);
  free(nrm);
}

static int init_common(void) {
  static const char *vs_frag =
      "attribute vec4 aPosition;\n"
      "attribute vec3 aNormal;\n"
      "uniform mat4 uMVP;\n"
      "uniform mat4 uModel;\n"
      "varying vec3 vN;\n"
      "void main() {\n"
      "  vN = mat3(uModel) * aNormal;\n"
      "  gl_Position = uMVP * (uModel * aPosition);\n"
      "}\n";
  static const char *fs_frag =
      "precision mediump float;\n"
      "varying vec3 vN;\n"
      "uniform vec3 uLight;\n"
      "uniform vec4 uColor;\n"
      "void main() {\n"
      "  vec3 N = normalize(vN);\n"
      "  float d = max(dot(uLight, N), 0.0);\n"
      "  vec4 c = uColor * d + vec4(0.4,0.4,0.4,1.0) * pow(d, 20.0);\n"
      "  gl_FragColor = vec4(c.rgb + vec3(0.06,0.06,0.08), 1.0);\n"
      "}\n";
  static const char *vs_vert =
      "attribute vec4 aPosition;\n"
      "attribute vec3 aNormal;\n"
      "uniform mat4 uMVP;\n"
      "uniform mat4 uModel;\n"
      "uniform vec3 uLight;\n"
      "uniform vec4 uColor;\n"
      "varying vec4 vColor;\n"
      "void main() {\n"
      "  vec3 N = normalize(mat3(uModel) * aNormal);\n"
      "  float d = max(dot(uLight, N), 0.0);\n"
      "  vColor = uColor * d + vec4(0.4,0.4,0.4,1.0) * pow(d, 20.0);\n"
      "  gl_Position = uMVP * (uModel * aPosition);\n"
      "}\n";
  static const char *fs_vert =
      "precision mediump float;\n"
      "varying vec4 vColor;\n"
      "void main() {\n"
      "  gl_FragColor = vec4(vColor.rgb + vec3(0.06,0.06,0.08), 1.0);\n"
      "}\n";

  prog_frag = demo_program(vs_frag, fs_frag);
  prog_vert = demo_program(vs_vert, fs_vert);
  if (!prog_frag || !prog_vert) return -1;
  f_mvp = glGetUniformLocation(prog_frag, "uMVP");
  f_light = glGetUniformLocation(prog_frag, "uLight");
  f_color = glGetUniformLocation(prog_frag, "uColor");
  f_pos = glGetAttribLocation(prog_frag, "aPosition");
  f_normal = glGetAttribLocation(prog_frag, "aNormal");
  v_mvp = glGetUniformLocation(prog_vert, "uMVP");
  v_light = glGetUniformLocation(prog_vert, "uLight");
  v_color = glGetUniformLocation(prog_vert, "uColor");
  v_pos = glGetAttribLocation(prog_vert, "aPosition");
  v_normal = glGetAttribLocation(prog_vert, "aNormal");
  make_meshes();
  glEnable(GL_CULL_FACE);
  return 0;
}

static int g_frag;

static int init(void) { return init_common(); }

static void draw(float t) {
  mat4 proj, view, pv, model;
  GLuint prog = g_frag ? prog_frag : prog_vert;
  GLint l_mvp = g_frag ? f_mvp : v_mvp;
  GLint l_light = g_frag ? f_light : v_light;
  GLint l_color = g_frag ? f_color : v_color;
  GLint l_pos = g_frag ? f_pos : v_pos;
  GLint l_nrm = g_frag ? f_normal : v_normal;

  mat4_perspective(&proj, 45.0f, 640.0f / 480.0f, 1.0f, 100.0f);
  mat4_identity(&view);
  mat4_translate(&view, 0.0f, 0.0f, -8.0f);
  mat4_mul(&pv, &proj, &view);
  mat4_identity(&model);
  mat4_rotate(&model, t * 25.0f, 0.0f, 1.0f, 0.0f);
  mat4_rotate(&model, t * 13.0f, 1.0f, 0.0f, 0.0f);

  glUseProgram(prog);
  glUniformMatrix4fv(l_mvp, 1, GL_FALSE, pv.m);
  glUniformMatrix4fv(glGetUniformLocation(prog, "uModel"), 1, GL_FALSE,
                     model.m);
  glUniform3f(l_light, 0.57737f, 0.57735f, 0.57735f);
  glUniform4f(l_color, 0.85f, 0.18f, 0.12f, 1.0f);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_pos);
  glEnableVertexAttribArray(l_pos);
  glVertexAttribPointer(l_pos, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_nrm);
  glEnableVertexAttribArray(l_nrm);
  glVertexAttribPointer(l_nrm, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glDrawArrays(GL_TRIANGLES, 0, nverts);
}

int lighting_run(int per_fragment) {
  g_frag = per_fragment;
  return demo_run(init, draw);
}
