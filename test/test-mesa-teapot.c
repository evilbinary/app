/* test-mesa-teapot.c - gallium 茶壶，支持 direct / tfu 两种 present
 *
 * 用法：
 *   /bin/test-mesa-teapot direct [frames]   # 零拷贝直扫，默认 300 帧
 *   /bin/test-mesa-teapot tfu    [frames]   # 离屏 + TFU present
 *
 * 几何是程序化生成的：壶身/壶盖/壶嘴/壶柄/壶钮。CPU 每帧做 Y 轴旋转、
 * 透视投影和简单方向光，然后上传顶点。这样不依赖矩阵 uniform，先用最小
 * 状态集把「能动起来的 3D 茶壶」跑出来；后续再把 MVP 移到 VS。
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "client.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "util/u_inlines.h"
#include "util/u_draw.h"
#include "util/u_simple_shaders.h"
#include "util/xmlconfig.h"
#include "frontend/winsys_handle.h"
#include "v3d_drm_public.h"
#include "broadcom/common/v3d_limits.h"
#include "v3d/v3d_screen.h"
#include "v3d/v3d_resource.h"
#include "v3d/v3d_bufmgr.h"

#define GPU_LOW_SUBMIT_TFU 122
#define GPU_LOW_SCREEN_PARAM 141

typedef struct {
  unsigned int src_handle, src_offset;
  unsigned int dst_handle, dst_offset;
  unsigned int w, h;
  unsigned int src_fmt, dst_fmt;
  unsigned int flags;
} gpu_submit_tfu_req;

#define MAX_VERTS 24000
#define VSTRIDE 20u /* float4 position + RGBA8 color */

static float g_mesh_p[MAX_VERTS][3];
static float g_mesh_n[MAX_VERTS][3];
static uint8_t g_mesh_c[MAX_VERTS][3];
static int g_mesh_count;

static uint8_t g_vdata[MAX_VERTS * VSTRIDE];

#define BO_VA_BASE 0x52000000UL
static unsigned long g_bo_next = BO_VA_BASE;

int bridge_bo_alloc(unsigned int size, unsigned int* pa, unsigned int* cpu_va) {
  unsigned int p = 0, s = 0;
  foot_cap_t cap = foot_cap_alloc_arena(size, 0, 0, &p, &s);
  if (cap == FOOT_CAP_NONE || s < size) {
    return -1;
  }
  unsigned long va = g_bo_next;
  g_bo_next += (s + 0xFFFu) & ~0xFFFUL;
  if (foot_cap_map(cap, (void*)va, 0, s) != 0) {
    return -1;
  }
  *pa = p;
  *cpu_va = (unsigned int)va;
  return 0;
}

static struct pipe_screen* make_screen(int* fd_out) {
  static driOptionInfo info_entry;
  static driOptionValue value_entry;
  static driOptionCache info_cache;
  static driOptionCache options_cache;
  memset(&info_entry, 0, sizeof(info_entry));
  memset(&value_entry, 0, sizeof(value_entry));
  memset(&info_cache, 0, sizeof(info_cache));
  memset(&options_cache, 0, sizeof(options_cache));
  info_cache.info = &info_entry;
  info_cache.values = &value_entry;
  info_cache.tableSize = 0;
  options_cache.info = &info_entry;
  options_cache.values = &value_entry;
  options_cache.tableSize = 0;

  static struct pipe_screen_config config;
  memset(&config, 0, sizeof(config));
  config.options = &options_cache;
  config.options_info = &info_cache;

  int fd = open("/dev/null", O_RDWR);
  if (fd < 0) return NULL;
  struct pipe_screen* screen = v3d_drm_screen_create(fd, &config);
  if (screen == NULL) {
    close(fd);
    return NULL;
  }
  *fd_out = fd;
  return screen;
}

/* ---------------- 网格生成 ---------------- */
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
  /* u = normalize(cross(axis, (0,0,1))) */
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
  /* spout: two tapered segments */
  gen_cylinder(0.92f, 0.80f, 0, 1.28f, 0.88f, 0, 0.16f, 0.12f, 12, 200, 120, 60);
  gen_cylinder(1.28f, 0.88f, 0, 1.62f, 1.38f, 0, 0.12f, 0.07f, 12, 200, 120, 60);
  /* handle: half torus on -X */
  gen_torus(-1.00f, 0.88f, 0, 0.34f, 0.085f, (float)M_PI * 0.5f,
            (float)M_PI * 1.5f, 14, 8, 190, 110, 55);
}

/* ---------------- 每帧 CPU 变换 ---------------- */
static int build_frame(float angle, float aspect, float* vp_w, float* vp_h) {
  (void)vp_w;
  (void)vp_h;
  float ca = cosf(angle), sa = sinf(angle);
  const float f = 2.414f; /* 1/tan(22.5deg) */
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
    /* v3d framebuffer 原点在左上：正的 NDC Y 会落到屏幕下方，
     * 这里取反，保证壶盖朝上。 */
    float yn = -f * y * inv;
    float zn = 0.5f;

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
    fp[0] = xn; fp[1] = yn; fp[2] = zn; fp[3] = 1.0f;
    p[16] = (uint8_t)r;
    p[17] = (uint8_t)g;
    p[18] = (uint8_t)b;
    p[19] = 255;
    n++;
  }
  return n;
}

int main(int argc, char** argv) {
  int direct = (argc > 1 && strcmp(argv[1], "direct") == 0);
  int frames = (argc > 2) ? atoi(argv[2]) : 300;
  const char* mode = direct ? "direct" : "tfu";
  const unsigned int W = 640, H = 480;
  int fd = -1;
  int fail = 0;

  if (frames <= 0) frames = 300;
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("=== test-mesa-teapot (mode=%s frames=%d) ===\n", mode, frames);

  build_teapot();
  printf("mesh: %d vertices\n", g_mesh_count);

  struct pipe_screen* screen = make_screen(&fd);
  if (screen == NULL) return 1;
  struct pipe_context* ctx = screen->context_create(screen, NULL, 0);
  if (ctx == NULL) {
    screen->destroy(screen);
    close(fd);
    return 1;
  }
  client_t* gc = client_get("gpu");

  struct pipe_resource* res = NULL;
  unsigned int fb_w = W, fb_h = H;
  unsigned int vp_w = W, vp_h = H;

  if (direct) {
    unsigned int params[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    long v[8];
    for (int i = 0; i < 8; i++) {
      v[i] = gc ? (long)client_call_ex(gc, (void*)GPU_LOW_SCREEN_PARAM,
                                       &params[i], sizeof(params[i]), 5000)
                : -1;
    }
    printf("screen: gva=0x%08x %ux%u pitch=%u\n", (unsigned)v[0],
           (unsigned)v[2], (unsigned)v[3], (unsigned)v[4]);
    if (v[0] < 0 || v[2] <= 0 || v[4] <= 0) {
      fail++;
      goto out_ctx;
    }
    struct pipe_resource stempl;
    memset(&stempl, 0, sizeof(stempl));
    stempl.target = PIPE_TEXTURE_2D;
    stempl.format = PIPE_FORMAT_B5G6R5_UNORM;
    stempl.width0 = (unsigned)v[2];
    stempl.height0 = (unsigned)v[3];
    stempl.depth0 = 1;
    stempl.array_size = 1;
    stempl.bind = PIPE_BIND_RENDER_TARGET | PIPE_BIND_LINEAR;
    struct winsys_handle wh;
    memset(&wh, 0, sizeof(wh));
    wh.type = WINSYS_HANDLE_TYPE_SHARED;
    wh.handle = 1;
    wh.stride = (unsigned)v[4];
    wh.modifier = 0;
    res = screen->resource_from_handle(screen, &stempl, &wh, 0);
    fb_w = vp_w = (unsigned)v[2];
    fb_h = vp_h = (unsigned)v[3];
  } else {
    struct pipe_resource templ;
    memset(&templ, 0, sizeof(templ));
    templ.target = PIPE_TEXTURE_2D;
    templ.format = PIPE_FORMAT_B5G6R5_UNORM;
    templ.width0 = W;
    templ.height0 = H;
    templ.depth0 = 1;
    templ.array_size = 1;
    templ.bind = PIPE_BIND_RENDER_TARGET | PIPE_BIND_LINEAR;
    res = screen->resource_create(screen, &templ);
    fb_w = vp_w = W;
    fb_h = vp_h = H;
  }
  printf("fb resource = %p %s (%ux%u)\n", (void*)res, res ? "OK" : "FAIL",
         fb_w, fb_h);
  if (res == NULL) {
    fail++;
    goto out_ctx;
  }
  struct v3d_resource* vr = v3d_resource(res);
  printf("fb bo handle=%u stride=%u\n", vr->bo->handle, vr->slices[0].stride);

  struct pipe_framebuffer_state fb;
  memset(&fb, 0, sizeof(fb));
  fb.width = fb_w;
  fb.height = fb_h;
  fb.nr_cbufs = 1;
  fb.cbufs[0].format = res->format;
  fb.cbufs[0].texture = res;
  ctx->set_framebuffer_state(ctx, &fb);

  /* dynamic vertex buffer */
  struct pipe_resource vb_templ;
  memset(&vb_templ, 0, sizeof(vb_templ));
  vb_templ.target = PIPE_BUFFER;
  vb_templ.format = PIPE_FORMAT_R8_UNORM;
  vb_templ.width0 = MAX_VERTS * VSTRIDE;
  vb_templ.height0 = 1;
  vb_templ.depth0 = 1;
  vb_templ.array_size = 1;
  vb_templ.bind = PIPE_BIND_VERTEX_BUFFER;
  struct pipe_resource* vbuf = screen->resource_create(screen, &vb_templ);
  if (vbuf == NULL) {
    fail++;
    goto out_res;
  }
  struct pipe_vertex_buffer vb;
  memset(&vb, 0, sizeof(vb));
  vb.is_user_buffer = false;
  vb.buffer.resource = vbuf;
  ctx->set_vertex_buffers(ctx, 1, &vb);

  struct pipe_vertex_element ve[2];
  memset(ve, 0, sizeof(ve));
  ve[0].src_offset = 0;
  ve[0].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
  ve[0].src_stride = VSTRIDE;
  ve[0].vertex_buffer_index = 0;
  ve[1].src_offset = 16;
  ve[1].src_format = PIPE_FORMAT_R8G8B8A8_UNORM;
  ve[1].src_stride = VSTRIDE;
  ve[1].vertex_buffer_index = 0;
  void* ve_state = ctx->create_vertex_elements_state(ctx, 2, ve);
  ctx->bind_vertex_elements_state(ctx, ve_state);

  const enum tgsi_semantic sem_names[2] = {TGSI_SEMANTIC_POSITION,
                                           TGSI_SEMANTIC_COLOR};
  const unsigned sem_indices[2] = {0, 0};
  void* vs = util_make_vertex_passthrough_shader(ctx, 2, sem_names,
                                                 sem_indices, false);
  void* fs = util_make_fragment_passthrough_shader(
      ctx, TGSI_SEMANTIC_COLOR, TGSI_INTERPOLATE_PERSPECTIVE, false);
  if (vs == NULL || fs == NULL) {
    fail++;
    goto out_vbuf;
  }
  ctx->bind_vs_state(ctx, vs);
  ctx->bind_fs_state(ctx, fs);

  struct pipe_rasterizer_state rs;
  memset(&rs, 0, sizeof(rs));
  rs.cull_face = PIPE_FACE_NONE;
  rs.fill_front = PIPE_POLYGON_MODE_FILL;
  rs.fill_back = PIPE_POLYGON_MODE_FILL;
  rs.front_ccw = 1;
  rs.scissor = 1;
  rs.half_pixel_center = 1;
  rs.bottom_edge_rule = 1;
  rs.depth_clip_near = 1;
  rs.depth_clip_far = 1;
  void* rs_state = ctx->create_rasterizer_state(ctx, &rs);
  ctx->bind_rasterizer_state(ctx, rs_state);

  struct pipe_depth_stencil_alpha_state dsa;
  memset(&dsa, 0, sizeof(dsa));
  dsa.depth_func = PIPE_FUNC_ALWAYS;
  void* dsa_state = ctx->create_depth_stencil_alpha_state(ctx, &dsa);
  ctx->bind_depth_stencil_alpha_state(ctx, dsa_state);

  struct pipe_blend_state blend;
  memset(&blend, 0, sizeof(blend));
  blend.rt[0].colormask = PIPE_MASK_RGBA;
  void* blend_state = ctx->create_blend_state(ctx, &blend);
  ctx->bind_blend_state(ctx, blend_state);

  struct pipe_viewport_state vp;
  memset(&vp, 0, sizeof(vp));
  vp.scale[0] = vp_w / 2.0f;
  vp.scale[1] = vp_h / 2.0f;
  vp.scale[2] = 1.0f;
  vp.translate[0] = vp_w / 2.0f;
  vp.translate[1] = vp_h / 2.0f;
  ctx->set_viewport_states(ctx, 0, 1, &vp);

  struct pipe_scissor_state scissor;
  scissor.minx = 0;
  scissor.miny = 0;
  scissor.maxx = vp_w;
  scissor.maxy = vp_h;
  ctx->set_scissor_states(ctx, 0, 1, &scissor);

  union pipe_color_union bg;
  memset(&bg, 0, sizeof(bg));
  bg.f[2] = 0.18f;
  bg.f[3] = 1.0f;

  float aspect = (float)vp_w / (float)vp_h;
  for (int f = 0; f < frames; f++) {
    float angle = (float)f * 0.045f; /* ~2.6 deg/frame */
    int n = build_frame(angle, aspect, &vp_w, &vp_h);
    ctx->buffer_subdata(ctx, vbuf, 0, 0, (unsigned)(n * VSTRIDE), g_vdata);
    ctx->clear(ctx, PIPE_CLEAR_COLOR0, 0xFu, 0u, NULL, &bg, 0.0, 0);
    util_draw_arrays(ctx, MESA_PRIM_TRIANGLES, 0, n);
    ctx->flush(ctx, NULL, 0);

    if (!direct) {
      gpu_submit_tfu_req tr;
      memset(&tr, 0, sizeof(tr));
      tr.src_handle = vr->bo->handle;
      tr.dst_handle = 0;
      tr.w = W;
      tr.h = H;
      tr.src_fmt = 6;
      tr.dst_fmt = 6;
      if (gc) {
        client_call_ex(gc, (void*)GPU_LOW_SUBMIT_TFU, &tr, sizeof(tr), 10000);
      }
    }

    if ((f & 15) == 0) {
      printf("frame %d/%d verts=%d angle=%.2f\n", f, frames, n, angle);
    }
    usleep(33000);
  }

  printf("teapot done: %d frames, present=%s\n", frames, mode);

out_vbuf:
  pipe_resource_reference(&vbuf, NULL);
out_res:
  pipe_resource_reference(&res, NULL);
out_ctx:
  ctx->destroy(ctx);
  screen->destroy(screen);
  close(fd);
  printf("=== %s (fail=%d) ===\n", fail == 0 ? "PASS" : "FAIL", fail);
  return fail;
}
