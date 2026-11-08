/* toyball - mesa-demos CH11-toyball 的 Wayland/GLES2 重写。
 *
 * 原 demo 是 GLSL 1.10 + glutSolidSphere；这里用 GLES2 球体网格，
 * 片元着色器保留原来的 half-space 星形/条纹图案和逐像素光照。
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "demo.h"

static GLuint prog, vbo;
static GLint loc_mvp, loc_model, loc_texrot, loc_pos;
static int nverts;

static int init(void) {
  static const char *vs =
      "attribute vec4 aPosition;\n"
      "uniform mat4 uMVP;\n"
      "uniform mat4 uModel;\n"
      "uniform float uTexRot;\n"
      "varying vec3 vP;\n"
      "void main() {\n"
      "  float c = cos(uTexRot), s = sin(uTexRot);\n"
      "  vec3 p = vec3(c*aPosition.x + s*aPosition.z, aPosition.y,\n"
      "                -s*aPosition.x + c*aPosition.z);\n"
      "  vP = p;\n"
      "  gl_Position = uMVP * (uModel * vec4(p, 1.0));\n"
      "}\n";
  static const char *fs =
      "precision mediump float;\n"
      "varying vec3 vP;\n"
      "void main() {\n"
      "  vec3 p = normalize(vP);\n"
      "  float fw = 0.005;\n"
      "  float inorout = -3.0;\n"
      "  float d0 = smoothstep(-fw, fw, dot(p, vec3(1.0,0.0,0.0)) + 0.2);\n"
      "  float d1 = smoothstep(-fw, fw, dot(p, vec3(0.309017,0.951057,0.0)) + 0.2);\n"
      "  float d2 = smoothstep(-fw, fw, dot(p, vec3(-0.809017,0.587785,0.0)) + 0.2);\n"
      "  float d3 = smoothstep(-fw, fw, dot(p, vec3(-0.809017,-0.587785,0.0)) + 0.2);\n"
      "  inorout += d0 + d1 + d2 + d3;\n"
      "  float d4 = smoothstep(-fw, fw, dot(p, vec3(0.309117,-0.951057,0.0)) + 0.2);\n"
      "  inorout += d4;\n"
      "  inorout = clamp(inorout, 0.0, 1.0);\n"
      "  float stripe = smoothstep(-fw, fw, 0.3 - abs(p.z));\n"
      "  vec3 red = vec3(0.6,0.0,0.0);\n"
      "  vec3 yellow = vec3(0.6,0.5,0.0);\n"
      "  vec3 blue = vec3(0.0,0.3,0.6);\n"
      "  vec3 c = mix(yellow, red, inorout);\n"
      "  c = mix(c, blue, stripe);\n"
      "  vec3 L = normalize(vec3(0.57737,0.57735,0.57735));\n"
      "  float diff = max(dot(L, p), 0.0);\n"
      "  c *= 0.2 + 0.8 * diff;\n"
      "  vec3 H = normalize(vec3(0.32506,0.32506,0.88808));\n"
      "  float spec = pow(max(dot(H, p), 0.0), 60.0);\n"
      "  c += vec3(0.4) * spec;\n"
      "  gl_FragColor = vec4(c, 1.0);\n"
      "}\n";
  float *verts;
  int i, count;

  prog = demo_program(vs, fs);
  if (!prog) return -1;
  loc_mvp = glGetUniformLocation(prog, "uMVP");
  loc_model = glGetUniformLocation(prog, "uModel");
  loc_texrot = glGetUniformLocation(prog, "uTexRot");
  loc_pos = glGetAttribLocation(prog, "aPosition");

  demo_sphere(2.0f, 32, 20, &verts, &count);
  if (!verts || count <= 0) return -1;
  nverts = count;
  /* demo_sphere 是 position+normal 交错；本 demo 只需要 position，
   * 压紧成 3 floats/vertex。 */
  {
    float *packed = malloc((size_t)count * 3 * sizeof(float));
    if (!packed) return -1;
    for (i = 0; i < count; i++) {
      packed[i * 3 + 0] = verts[i * 6 + 0];
      packed[i * 3 + 1] = verts[i * 6 + 1];
      packed[i * 3 + 2] = verts[i * 6 + 2];
    }
    free(verts);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)count * 3 * sizeof(float),
                 packed, GL_STATIC_DRAW);
    free(packed);
  }
  glEnable(GL_CULL_FACE);
  return 0;
}

static void draw(float t) {
  mat4 proj, view, pv, model;
  if (!prog) return;
  mat4_perspective(&proj, 45.0f, 640.0f / 480.0f, 1.0f, 100.0f);
  mat4_identity(&view);
  mat4_translate(&view, 0.0f, 0.0f, -8.0f);
  mat4_mul(&pv, &proj, &view);
  mat4_identity(&model);
  mat4_rotate(&model, sinf(t * 0.4f) * 25.0f, 1.0f, 0.0f, 0.0f);

  glUseProgram(prog);
  glUniformMatrix4fv(loc_mvp, 1, GL_FALSE, pv.m);
  glUniformMatrix4fv(loc_model, 1, GL_FALSE, model.m);
  glUniform1f(loc_texrot, t * 2.0f);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glEnableVertexAttribArray(loc_pos);
  glVertexAttribPointer(loc_pos, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                        (void *)0);
  glDrawArrays(GL_TRIANGLES, 0, nverts);
}

int main(void) { demo_set_title("Toyball"); return demo_run(init, draw); }
