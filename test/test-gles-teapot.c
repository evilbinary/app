/* test-gles-teapot.c - Mesa EGL/GLES2 茶壶：glDrawArrays 全链路
 *
 * 流程：EGL device + GLES2 context + FBO -> GLSL 着色器 -> 旋转茶壶
 *       -> 两个不同角度 glDrawArrays -> glReadPixels 验证。
 *
 * 运行前：/lib/libv3d_dri.so、/lib/libEGL.so、/lib/libGLESv2.so，
 *         且 gpud 已启动。
 */
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "client.h"

#ifndef EGL_PLATFORM_DEVICE_EXT
#define EGL_PLATFORM_DEVICE_EXT 0x313F
#endif

#define GPU_LOW_SCREEN_PARAM 141
#define GPU_LOW_BO_CREATE 110
#define GPU_LOW_SUBMIT_TFU 122

/* 与 foot/driver/gpu/gpu_backend.h 的 gpu_submit_tfu_req 字节一致 */
typedef struct {
  unsigned int src_handle, src_offset;
  unsigned int dst_handle, dst_offset;
  unsigned int w, h;
  unsigned int src_fmt, dst_fmt;
  unsigned int flags;
} gpu_submit_tfu_req;

typedef struct {
  unsigned int pa, size, flags;
} gpu_bo_create_req;

#define W 640
#define H 480
#define MAX_VERTS 24000
#define VSTRIDE 16 /* float3 position(NDC) + RGBA8 color */

static float g_mesh_p[MAX_VERTS][3];
static float g_mesh_n[MAX_VERTS][3];
static uint8_t g_mesh_c[MAX_VERTS][3];
static int g_mesh_count;

static uint8_t g_vdata[MAX_VERTS * VSTRIDE];
static uint8_t g_pixels[W * H * 4];

/* ---------------- 网格生成（与 test-mesa-teapot 相同） ---------------- */
static void add_vert(float x, float y, float z, float nx, float ny, float nz,
                     int r, int g, int b) {
  if (g_mesh_count >= MAX_VERTS) return;
  int i = g_mesh_count++;
  g_mesh_p[i][0] = x;
  g_mesh_p[i][1] = y;
  g_mesh_p[i][2] = z;
  g_mesh_n[i][0] = nx;
  g_mesh_n[i][1] = ny;
  g_mesh_n[i][2] = nz;
  g_mesh_c[i][0] = (uint8_t)r;
  g_mesh_c[i][1] = (uint8_t)g;
  g_mesh_c[i][2] = (uint8_t)b;
}

static void add_tri(float ax, float ay, float az, float bx, float by, float bz,
                    float cx, float cy, float cz, int r, int g, int b) {
  float ux = bx - ax, uy = by - ay, uz = bz - az;
  float vx = cx - ax, vy = cy - ay, vz = cz - az;
  float nx = uy * vz - uz * vy;
  float ny = uz * vx - ux * vz;
  float nz = ux * vy - uy * vx;
  float len = sqrtf(nx * nx + ny * ny + nz * nz);
  if (len < 1e-6f) return;
  nx /= len; ny /= len; nz /= len;
  add_vert(ax, ay, az, nx, ny, nz, r, g, b);
  add_vert(bx, by, bz, nx, ny, nz, r, g, b);
  add_vert(cx, cy, cz, nx, ny, nz, r, g, b);
}

static void gen_lathe(const float* prof, int np, int seg, int r, int g, int b) {
  for (int i = 0; i < np - 1; i++) {
    float r0 = prof[2 * i], y0 = prof[2 * i + 1];
    float r1 = prof[2 * i + 2], y1 = prof[2 * i + 3];
    for (int j = 0; j < seg; j++) {
      float a0 = 2.0f * (float)M_PI * j / seg;
      float a1 = 2.0f * (float)M_PI * (j + 1) / seg;
      float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
      float p00x = r0 * c0, p00y = y0, p00z = r0 * s0;
      float p01x = r0 * c1, p01y = y0, p01z = r0 * s1;
      float p10x = r1 * c0, p10y = y1, p10z = r1 * s0;
      float p11x = r1 * c1, p11y = y1, p11z = r1 * s1;
      add_tri(p00x, p00y, p00z, p10x, p10y, p10z, p11x, p11y, p11z, r, g, b);
      add_tri(p00x, p00y, p00z, p11x, p11y, p11z, p01x, p01y, p01z, r, g, b);
    }
  }
}

static void gen_bottom_cap(float radius, float y, int seg, int r, int g, int b) {
  for (int j = 0; j < seg; j++) {
    float a0 = 2.0f * (float)M_PI * j / seg;
    float a1 = 2.0f * (float)M_PI * (j + 1) / seg;
    add_tri(0, y, 0, radius * cosf(a0), y, radius * sinf(a0),
            radius * cosf(a1), y, radius * sinf(a1), r, g, b);
  }
}

static void gen_sphere(float cx, float cy, float cz, float rad, int slices,
                       int stacks, int r, int g, int b) {
  for (int i = 0; i < stacks; i++) {
    float p0 = (float)M_PI * i / stacks - (float)M_PI / 2.0f;
    float p1 = (float)M_PI * (i + 1) / stacks - (float)M_PI / 2.0f;
    for (int j = 0; j < slices; j++) {
      float t0 = 2.0f * (float)M_PI * j / slices;
      float t1 = 2.0f * (float)M_PI * (j + 1) / slices;
      float cp0 = cosf(p0), sp0 = sinf(p0), cp1 = cosf(p1), sp1 = sinf(p1);
      float q00x = cx + rad * cp0 * cosf(t0), q00y = cy + rad * sp0,
            q00z = cz + rad * cp0 * sinf(t0);
      float q01x = cx + rad * cp0 * cosf(t1), q01y = cy + rad * sp0,
            q01z = cz + rad * cp0 * sinf(t1);
      float q10x = cx + rad * cp1 * cosf(t0), q10y = cy + rad * sp1,
            q10z = cz + rad * cp1 * sinf(t0);
      float q11x = cx + rad * cp1 * cosf(t1), q11y = cy + rad * sp1,
            q11z = cz + rad * cp1 * sinf(t1);
      add_tri(q00x, q00y, q00z, q10x, q10y, q10z, q11x, q11y, q11z, r, g, b);
      add_tri(q00x, q00y, q00z, q11x, q11y, q11z, q01x, q01y, q01z, r, g, b);
    }
  }
}

static void gen_cylinder(float x0, float y0, float z0, float x1, float y1,
                         float z1, float rad0, float rad1, int sides, int r,
                         int g, int b) {
  float ax = x1 - x0, ay = y1 - y0, az = z1 - z0;
  float al = sqrtf(ax * ax + ay * ay + az * az);
  if (al < 1e-6f) return;
  ax /= al; ay /= al; az /= al;
  float ux = 0, uy = 0, uz = 1;
  if (fabsf(ay) > 0.9f) { ux = 1; uy = 0; uz = 0; }
  float cx = ay * uz - az * uy;
  float cy = az * ux - ax * uz;
  float cz = ax * uy - ay * ux;
  float cl = sqrtf(cx * cx + cy * cy + cz * cz);
  cx /= cl; cy /= cl; cz /= cl;
  float vx = ay * cz - az * cy;
  float vy = az * cx - ax * cz;
  float vz = ax * cy - ay * cx;
  for (int j = 0; j < sides; j++) {
    float a0 = 2.0f * (float)M_PI * j / sides;
    float a1 = 2.0f * (float)M_PI * (j + 1) / sides;
    float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
    float q00x = x0 + rad0 * (cx * c0 + vx * s0);
    float q00y = y0 + rad0 * (cy * c0 + vy * s0);
    float q00z = z0 + rad0 * (cz * c0 + vz * s0);
    float q01x = x0 + rad0 * (cx * c1 + vx * s1);
    float q01y = y0 + rad0 * (cy * c1 + vy * s1);
    float q01z = z0 + rad0 * (cz * c1 + vz * s1);
    float q10x = x1 + rad1 * (cx * c0 + vx * s0);
    float q10y = y1 + rad1 * (cy * c0 + vy * s0);
    float q10z = z1 + rad1 * (cz * c0 + vz * s0);
    float q11x = x1 + rad1 * (cx * c1 + vx * s1);
    float q11y = y1 + rad1 * (cy * c1 + vy * s1);
    float q11z = z1 + rad1 * (cz * c1 + vz * s1);
    add_tri(q00x, q00y, q00z, q10x, q10y, q10z, q11x, q11y, q11z, r, g, b);
    add_tri(q00x, q00y, q00z, q11x, q11y, q11z, q01x, q01y, q01z, r, g, b);
  }
}

static void gen_torus(float cx, float cy, float cz, float R, float rt,
                      float a_start, float a_end, int path_seg, int tube_seg,
                      int r, int g, int b) {
  for (int i = 0; i < path_seg; i++) {
    float a0 = a_start + (a_end - a_start) * i / path_seg;
    float a1 = a_start + (a_end - a_start) * (i + 1) / path_seg;
    for (int j = 0; j < tube_seg; j++) {
      float b0 = 2.0f * (float)M_PI * j / tube_seg;
      float b1 = 2.0f * (float)M_PI * (j + 1) / tube_seg;
      float ca0 = cosf(a0), sa0 = sinf(a0), ca1 = cosf(a1), sa1 = sinf(a1);
      float cb0 = cosf(b0), sb0 = sinf(b0), cb1 = cosf(b1), sb1 = sinf(b1);
      float q00x = cx + (R + rt * cb0) * ca0;
      float q00y = cy + (R + rt * cb0) * sa0;
      float q00z = cz + rt * sb0;
      float q01x = cx + (R + rt * cb1) * ca0;
      float q01y = cy + (R + rt * cb1) * sa0;
      float q01z = cz + rt * sb1;
      float q10x = cx + (R + rt * cb0) * ca1;
      float q10y = cy + (R + rt * cb0) * sa1;
      float q10z = cz + rt * sb0;
      float q11x = cx + (R + rt * cb1) * ca1;
      float q11y = cy + (R + rt * cb1) * sa1;
      float q11z = cz + rt * sb1;
      add_tri(q00x, q00y, q00z, q10x, q10y, q10z, q11x, q11y, q11z, r, g, b);
      add_tri(q00x, q00y, q00z, q11x, q11y, q11z, q01x, q01y, q01z, r, g, b);
    }
  }
}

static void build_teapot(void) {
  static const float body[] = {
      0.55f, 0.00f, 0.85f, 0.10f, 1.00f, 0.35f, 1.05f, 0.70f,
      0.95f, 1.05f, 0.78f, 1.30f, 0.62f, 1.45f, 0.58f, 1.55f,
  };
  static const float lid[] = {
      0.58f, 1.55f, 0.55f, 1.62f, 0.45f, 1.72f,
      0.25f, 1.80f, 0.10f, 1.84f, 0.00f, 1.85f,
  };
  g_mesh_count = 0;
  gen_bottom_cap(0.55f, 0.0f, 32, 190, 105, 50);
  gen_lathe(body, (int)(sizeof(body) / sizeof(body[0]) / 2), 32, 200, 120, 60);
  gen_lathe(lid, (int)(sizeof(lid) / sizeof(lid[0]) / 2), 32, 210, 130, 70);
  gen_sphere(0, 1.93f, 0, 0.10f, 16, 10, 120, 60, 30);
  gen_cylinder(0.92f, 0.80f, 0, 1.28f, 0.88f, 0, 0.16f, 0.12f, 12, 200, 120, 60);
  gen_cylinder(1.28f, 0.88f, 0, 1.62f, 1.38f, 0, 0.12f, 0.07f, 12, 200, 120, 60);
  gen_torus(-1.00f, 0.88f, 0, 0.34f, 0.085f, (float)M_PI * 0.5f,
            (float)M_PI * 1.5f, 14, 8, 190, 110, 55);
}

/* ---------------- 每帧 CPU 变换 ---------------- */
static int build_frame(float angle, float aspect) {
  float ca = cosf(angle), sa = sinf(angle);
  const float f = 2.414f;
  const float cam_z = 4.2f;
  const float model_y = 0.95f;
  const float lx = 0.40f, ly = 0.80f, lz = 0.50f;
  int n = 0;
  for (int i = 0; i < g_mesh_count; i++) {
    float x = g_mesh_p[i][0];
    float y = g_mesh_p[i][1] - model_y;
    float z = g_mesh_p[i][2];
    float xr = x * ca + z * sa;
    float zr = -x * sa + z * ca;
    float zz = zr - cam_z;
    if (zz > -0.05f) continue;
    float inv = -1.0f / zz;
    float xn = f * xr * inv / aspect;
    float yn = -f * y * inv;

    float nx = g_mesh_n[i][0] * ca + g_mesh_n[i][2] * sa;
    float nz = -g_mesh_n[i][0] * sa + g_mesh_n[i][2] * ca;
    float ny = g_mesh_n[i][1];
    float d = fabsf(nx * lx + ny * ly + nz * lz);
    float lit = 0.25f + 0.75f * d;
    int r = (int)(g_mesh_c[i][0] * lit);
    int g = (int)(g_mesh_c[i][1] * lit);
    int b = (int)(g_mesh_c[i][2] * lit);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    uint8_t* p = g_vdata + (size_t)n * VSTRIDE;
    float* fp = (float*)p;
    fp[0] = xn;
    fp[1] = yn;
    fp[2] = 0.0f;
    p[12] = (uint8_t)r;
    p[13] = (uint8_t)g;
    p[14] = (uint8_t)b;
    p[15] = 255;
    n++;
  }
  return n;
}

/* ---------------- GL 辅助 ---------------- */
static GLuint compile_shader(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, NULL);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024];
    GLsizei len = 0;
    glGetShaderInfoLog(s, sizeof(log), &len, log);
    printf("shader compile failed: %.*s\n", (int)len, log);
    glDeleteShader(s);
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
  glBindAttribLocation(p, 0, "a_pos");
  glBindAttribLocation(p, 1, "a_color");
  glLinkProgram(p);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[1024];
    GLsizei len = 0;
    glGetProgramInfoLog(p, sizeof(log), &len, log);
    printf("program link failed: %.*s\n", (int)len, log);
    glDeleteProgram(p);
    return 0;
  }
  glDeleteShader(vs);
  glDeleteShader(fs);
  return p;
}

static void dump_egl_error(const char* where) {
  printf("%s failed, egl error 0x%x\n", where, eglGetError());
}

typedef struct {
  int nonbg;
  int minx, maxx, miny, maxy;
} frame_stats;

static frame_stats analyze_frame(const uint8_t* px) {
  frame_stats st = {0, W, -1, H, -1};
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      const uint8_t* p = px + ((size_t)y * W + x) * 4;
      int dx = (int)p[0] - 13;
      int dy = (int)p[1] - 13;
      int dz = (int)p[2] - 26;
      if (dx * dx + dy * dy + dz * dz > 900) {
        st.nonbg++;
        if (x < st.minx) st.minx = x;
        if (x > st.maxx) st.maxx = x;
        if (y < st.miny) st.miny = y;
        if (y > st.maxy) st.maxy = y;
      }
    }
  }
  return st;
}

/* ---------------- 上屏：direct（/dev/fb 直写）/ tfu（离屏 BO + TFU） ---------------- */
typedef struct {
  int sw, sh, pitch, cpp, size;
  int ox, oy;
  unsigned char* fb;      /* direct: /dev/fb 映射 */
  foot_cap_t cap;         /* tfu: 离屏 BO 的 arena cap */
  unsigned int bo_va, bo_size, bo;
  client_t* gc;
} present_t;

static int present_init(present_t* p, const char* mode, int w, int h) {
  memset(p, 0, sizeof(*p));
  p->gc = client_get("gpu");
  if (p->gc == NULL) {
    printf("present: client_get(gpu) failed\n");
    return -1;
  }
  long v[8];
  for (int i = 0; i < 8; i++) {
    unsigned int param = (unsigned int)i;
    v[i] = (long)client_call_ex(p->gc, (void*)GPU_LOW_SCREEN_PARAM, &param,
                                sizeof(param), 5000);
  }
  p->sw = (int)v[2];
  p->sh = (int)v[3];
  p->pitch = (int)v[4];
  p->cpp = (int)v[5];
  p->size = (int)v[7];
  if (p->sw <= 0 || p->sh <= 0 || p->pitch <= 0 || p->cpp <= 0 ||
      p->size <= 0) {
    printf("present: bad screen info %dx%d pitch=%d cpp=%d size=%d\n", p->sw,
           p->sh, p->pitch, p->cpp, p->size);
    return -1;
  }
  p->ox = ((p->sw - w) / 2) & ~1;
  p->oy = (p->sh - h) / 2;
  if (p->ox < 0) p->ox = 0;
  if (p->oy < 0) p->oy = 0;

  if (strcmp(mode, "direct") == 0) {
    int fd = open("/dev/fb", O_RDWR);
    if (fd < 0) {
      printf("present: open /dev/fb failed\n");
      return -1;
    }
    void* base = mmap(NULL, (size_t)p->size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    close(fd);
    if (base == MAP_FAILED) {
      printf("present: mmap /dev/fb failed\n");
      return -1;
    }
    p->fb = (unsigned char*)base;
  } else {
    unsigned int sz = 0, pa = 0;
    p->cap = foot_cap_alloc_arena((unsigned long)w * h * 2, 0, 0, &pa, &sz);
    if (p->cap == FOOT_CAP_NONE) {
      printf("present: arena alloc failed\n");
      return -1;
    }
    void* va = (void*)0x54000000UL;
    if (foot_cap_map(p->cap, va, 0, sz) != 0) {
      printf("present: arena map failed\n");
      return -1;
    }
    p->bo_va = (unsigned int)(uintptr_t)va;
    p->bo_size = sz;
    gpu_bo_create_req req;
    memset(&req, 0, sizeof(req));
    req.pa = pa;
    req.size = sz;
    long handle = (long)client_call_ex(p->gc, (void*)GPU_LOW_BO_CREATE, &req,
                                       sizeof(req), 60000);
    if (handle <= 0) {
      printf("present: BO create failed (%ld)\n", handle);
      return -1;
    }
    p->bo = (unsigned int)handle;
  }
  return 0;
}

static void present_frame(present_t* p, const uint8_t* rgba, int w, int h) {
  if (p->fb != NULL) {
    uint16_t* base = (uint16_t*)p->fb;
    int pitch_px = p->pitch / p->cpp;
    for (int y = 0; y < h; y++) {
      uint16_t* row = base + (size_t)(p->oy + y) * pitch_px + p->ox;
      const uint8_t* src = rgba + (size_t)y * w * 4;
      for (int x = 0; x < w; x++) {
        uint8_t r = src[x * 4 + 0], g = src[x * 4 + 1], b = src[x * 4 + 2];
        row[x] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
      }
    }
    foot_cache_clean(p->fb, (char*)p->fb + p->size);
  } else {
    uint16_t* dst = (uint16_t*)(uintptr_t)p->bo_va;
    for (int y = 0; y < h; y++) {
      const uint8_t* src = rgba + (size_t)y * w * 4;
      for (int x = 0; x < w; x++) {
        uint8_t r = src[x * 4 + 0], g = src[x * 4 + 1], b = src[x * 4 + 2];
        dst[(size_t)y * w + x] =
            (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
      }
    }
    foot_cache_clean(dst, (char*)dst + (size_t)w * h * 2);
    gpu_submit_tfu_req tr;
    memset(&tr, 0, sizeof(tr));
    tr.src_handle = p->bo;
    tr.dst_handle = 0; /* 屏幕 */
    tr.w = (unsigned int)w;
    tr.h = (unsigned int)h;
    tr.src_fmt = 6; /* BGR565 */
    tr.dst_fmt = 6;
    tr.dst_offset = (unsigned int)p->oy * (unsigned int)p->pitch +
                    (unsigned int)p->ox * (unsigned int)p->cpp;
    long rc = (long)client_call_ex(p->gc, (void*)GPU_LOW_SUBMIT_TFU, &tr,
                                   sizeof(tr), 10000);
    if (rc != 0) {
      printf("present: TFU failed (%ld)\n", rc);
    }
  }
}

int main(int argc, char** argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("=== test-gles-teapot (EGL/GLES2 glDrawArrays) ===\n");

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

  EGLDisplay dpy = get_platform_display(EGL_PLATFORM_DEVICE_EXT,
                                        (void*)devs[0], NULL);
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

  GLuint tex = 0, fbo = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         tex, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    printf("FBO incomplete\n");
    return 1;
  }

  static const char* vs_src =
      "attribute vec3 a_pos;\n"
      "attribute vec4 a_color;\n"
      "varying vec4 v_color;\n"
      "void main() { gl_Position = vec4(a_pos.xy, 0.0, 1.0);"
      " v_color = a_color; }\n";
  static const char* fs_src =
      "precision mediump float;\n"
      "varying vec4 v_color;\n"
      "void main() { gl_FragColor = v_color; }\n";
  GLuint prog = link_program(vs_src, fs_src);
  if (!prog) return 1;
  glUseProgram(prog);

  GLuint vbo = 0;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, VSTRIDE, (void*)0);
  glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, VSTRIDE, (void*)12);

  build_teapot();
  printf("mesh: %d vertices\n", g_mesh_count);

  glViewport(0, 0, W, H);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  const float aspect = (float)W / (float)H;
  frame_stats stats[2];
  int verts[2] = {0, 0};

  for (int k = 0; k < 2; k++) {
    float angle = (k == 0) ? 0.0f : 1.15f;
    int n = build_frame(angle, aspect);
    verts[k] = n;

    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)n * VSTRIDE, g_vdata,
                 GL_STREAM_DRAW);
    glClearColor(0.05f, 0.05f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, n);
    glFinish();

    memset(g_pixels, 0, sizeof(g_pixels));
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, g_pixels);
    stats[k] = analyze_frame(g_pixels);
    printf("frame%d angle=%.2f verts=%d nonbg=%d bbox=[%d,%d]-[%d,%d]\n", k,
           angle, n, stats[k].nonbg, stats[k].minx, stats[k].miny,
           stats[k].maxx, stats[k].maxy);
  }

  /* 茶壶中心像素：应落在壶身上（棕色，R > B） */
  const uint8_t* cpx = g_pixels + ((size_t)(H / 2) * W + W / 2) * 4;
  printf("center pixel = %u %u %u %u\n", cpx[0], cpx[1], cpx[2], cpx[3]);

  int fail = 0;
  if (verts[0] < 1000 || verts[1] < 1000) {
    printf("too few vertices\n");
    fail = 1;
  }
  if (stats[0].nonbg < 5000 || stats[1].nonbg < 5000) {
    printf("teapot coverage too small\n");
    fail = 1;
  }
  if (stats[0].minx == stats[1].minx && stats[0].maxx == stats[1].maxx) {
    printf("rotation did not change silhouette\n");
    fail = 1;
  }
  if (!(cpx[0] > 30 && cpx[0] > cpx[2])) {
    printf("center pixel is not the teapot body\n");
    fail = 1;
  }

  eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(dpy, ctx);
  eglTerminate(dpy);
  printf("=== %s ===\n", fail ? "FAIL" : "PASS");
  return fail;
}
