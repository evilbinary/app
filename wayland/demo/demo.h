/* Wayland + EGL/GLES 演示框架公共头。
 *
 * 这块板子的 EGL 只有 OpenGL_ES（没有桌面 GL/GLX），所以 mesa-demos 里
 * 的 GLUT/固定管线 demo 不能直接跑；这里统一用 GLES2 + 顶点/片元着色器
 * 重写，画面效果对齐原 demo。
 */
#ifndef WL_DEMO_H
#define WL_DEMO_H

#include <GLES2/gl2.h>

typedef struct {
  float m[16]; /* column-major, OpenGL 约定 */
} mat4;

void mat4_identity(mat4 *m);
void mat4_perspective(mat4 *m, float fovy_deg, float aspect, float znear,
                      float zfar);
void mat4_translate(mat4 *m, float x, float y, float z);
void mat4_scale(mat4 *m, float x, float y, float z);
void mat4_rotate(mat4 *m, float angle_deg, float ax, float ay, float az);
void mat4_mul(mat4 *out, const mat4 *a, const mat4 *b);

/* 编译/链接一个 GLES2 program，失败返回 0 并打印日志。 */
GLuint demo_program(const char *vs, const char *fs);

/* 生成一个经纬球网格（位置+法线），返回顶点数；vbo 由调用者创建。 */
void demo_sphere(float radius, int slices, int stacks, float **verts,
                 int *vert_count);

/* 运行 Wayland 窗口 + EGL/GLES 渲染循环。
 * init() 返回 0 成功；draw(t) 在每帧被调用，t 单位秒。 */
int demo_run(int (*init)(void), void (*draw)(float t));

#endif
