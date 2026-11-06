/* test-mesa-present.c - 3D present 模式：direct（零拷贝直扫）/ tfu
 *
 * 目标：不用 EGL/GLES，直接用 Mesa v3d gallium 的 pipe_context 画一个
 * 绿色三角形，验证：
 *   - TGSI → NIR → QPU 的 shader 编译链
 *   - 顶点缓冲 / vertex elements / viewport / rasterizer 状态
 *   - 真正的 bin + render draw 路径（不只是 clear）
 *
 * 验收：640x480 线性 ARGB8888 BO，先清黑，再画绿色三角形；
 * 中心附近像素应为 0xFF00FF00。
 */
#include <stdio.h>
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
#include "frontend/winsys_handle.h"
#include "util/u_simple_shaders.h"
#include "util/xmlconfig.h"
#include "v3d_drm_public.h"
#include "broadcom/common/v3d_limits.h"
#include "v3d/v3d_screen.h"
#include "v3d/v3d_resource.h"
#include "v3d/v3d_bufmgr.h"

extern void* foot_v3d_bo_cpu_va(unsigned int handle);

/* 与 foot/driver/gpu/gpu_backend.h 的 gpu_submit_tfu_req 字节一致。
 * dst_handle=0 表示屏幕；同格式 TFU 直拷。 */
typedef struct {
  unsigned int src_handle, src_offset;
  unsigned int dst_handle, dst_offset;
  unsigned int w, h;
  unsigned int src_fmt, dst_fmt;
  unsigned int flags;
} gpu_submit_tfu_req;

typedef struct {
  unsigned int gva, pa, width, height, pitch, cpp, format, size;
} gpu_screen_info;

#define GPU_LOW_SCREEN_INFO 140
#define GPU_LOW_SCREEN_PARAM 141
#define GPU_LOW_SUBMIT_TFU 122

#define BO_VA_BASE 0x52000000UL
#define VSIZE 20u /* float4 pos(w=1) + RGBA8 color */
static unsigned long g_bo_next = BO_VA_BASE;

int bridge_bo_alloc(unsigned int size, unsigned int* pa, unsigned int* cpu_va) {
  unsigned int p = 0, s = 0;
  foot_cap_t cap = foot_cap_alloc_arena(size, 0, 0, &p, &s);
  if (cap == FOOT_CAP_NONE || s < size) {
    printf("bridge_bo_alloc: arena failed size=%u cap=%x\n", size, cap);
    return -1;
  }
  unsigned long va = g_bo_next;
  g_bo_next += (s + 0xFFFu) & ~0xFFFUL;
  if (foot_cap_map(cap, (void*)va, 0, s) != 0) {
    printf("bridge_bo_alloc: map failed va=0x%lx\n", va);
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


int main(int argc, char** argv) {
  const unsigned int W = 640, H = 480;
  int fd = -1;
  int fail = 0;
  int direct = (argc > 1 && strcmp(argv[1], "direct") == 0);
  const char* mode = direct ? "direct" : "tfu";

  setvbuf(stdout, NULL, _IONBF, 0);
  printf("=== test-mesa-present (mode=%s) ===\n", mode);

  struct pipe_screen* screen = make_screen(&fd);
  printf("screen = %p %s\n", (void*)screen, screen ? "OK" : "FAIL");
  if (screen == NULL) return 1;

  struct pipe_context* ctx = screen->context_create(screen, NULL, 0);
  printf("context = %p %s\n", (void*)ctx, ctx ? "OK" : "FAIL");
  if (ctx == NULL) {
    screen->destroy(screen);
    close(fd);
    return 1;
  }

  client_t* gc = client_get("gpu");
  printf("gpu client = %p %s\n", (void*)gc, gc ? "OK" : "FAIL");

  struct pipe_resource* res = NULL;
  struct v3d_resource* vr = NULL;
  unsigned int fb_w = W, fb_h = H;

  if (direct) {
    /* ---- 零拷贝直扫：把屏幕 framebuffer 包成 Mesa resource ---- */
    gpu_screen_info si;
    memset(&si, 0, sizeof(si));
    unsigned int params[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    long v[8];
    for (int i = 0; i < 8; i++) {
      v[i] = gc ? (long)client_call_ex(gc, (void*)GPU_LOW_SCREEN_PARAM,
                                       &params[i], sizeof(params[i]), 5000)
                : -1;
    }
    printf("screen_info gva=0x%08x pa=0x%08x %ux%u pitch=%u fmt=%u size=%u\n",
           (unsigned)v[0], (unsigned)v[1], (unsigned)v[2], (unsigned)v[3],
           (unsigned)v[4], (unsigned)v[6], (unsigned)v[7]);
    if (v[0] < 0 || v[2] <= 0 || v[4] <= 0 || v[7] <= 0) {
      fail++;
      goto out_ctx;
    }
    si.gva = (unsigned)v[0];
    si.pa = (unsigned)v[1];
    si.width = (unsigned)v[2];
    si.height = (unsigned)v[3];
    si.pitch = (unsigned)v[4];
    si.cpp = (unsigned)v[5];
    si.format = (unsigned)v[6];
    si.size = (unsigned)v[7];

    struct pipe_resource stempl;
    memset(&stempl, 0, sizeof(stempl));
    stempl.target = PIPE_TEXTURE_2D;
    stempl.format = PIPE_FORMAT_B5G6R5_UNORM;
    stempl.width0 = si.width;
    stempl.height0 = si.height;
    stempl.depth0 = 1;
    stempl.array_size = 1;
    stempl.bind = PIPE_BIND_RENDER_TARGET | PIPE_BIND_LINEAR;
    stempl.usage = PIPE_USAGE_DEFAULT;

    struct winsys_handle wh;
    memset(&wh, 0, sizeof(wh));
    wh.type = WINSYS_HANDLE_TYPE_SHARED;
    wh.handle = 1; /* libdrm 替身里的屏幕魔数 V3D_SCREEN_NAME */
    wh.stride = si.pitch;
    wh.offset = 0;
    wh.modifier = 0; /* DRM_FORMAT_MOD_LINEAR */
    res = screen->resource_from_handle(screen, &stempl, &wh, 0);
    printf("screen resource_from_handle = %p %s\n", (void*)res,
           res ? "OK" : "FAIL");
    fb_w = si.width;
    fb_h = si.height;
  } else {
    /* ---- TFU present：640x480 离屏 BGR565 ---- */
    struct pipe_resource templ;
    memset(&templ, 0, sizeof(templ));
    templ.target = PIPE_TEXTURE_2D;
    templ.format = PIPE_FORMAT_B5G6R5_UNORM;
    templ.width0 = W;
    templ.height0 = H;
    templ.depth0 = 1;
    templ.array_size = 1;
    templ.bind = PIPE_BIND_RENDER_TARGET | PIPE_BIND_LINEAR;
    templ.usage = PIPE_USAGE_DEFAULT;
    res = screen->resource_create(screen, &templ);
    printf("offscreen resource = %p %s\n", (void*)res, res ? "OK" : "FAIL");
  }
  if (res == NULL) {
    fail++;
    goto out_ctx;
  }

  vr = v3d_resource(res);
  printf("resource: handle=%u size=%u tiled=%d stride=%u tiling=%d fb=%ux%u\n",
         vr->bo->handle, vr->bo->size, vr->tiled, vr->slices[0].stride,
         vr->slices[0].tiling, fb_w, fb_h);

  struct pipe_framebuffer_state fb;
  memset(&fb, 0, sizeof(fb));
  fb.width = fb_w;
  fb.height = fb_h;
  fb.nr_cbufs = 1;
  fb.cbufs[0].format = res->format;
  fb.cbufs[0].texture = res;
  fb.cbufs[0].first_layer = 0;
  fb.cbufs[0].last_layer = 0;
  fb.cbufs[0].level = 0;
  ctx->set_framebuffer_state(ctx, &fb);

  /* ---- 顶点缓冲：3 个顶点，全绿色 ---- */
  float verts[3][4] = {
      {-0.8f, -0.8f, 0.5f, 1.0f},
      {0.8f, -0.8f, 0.5f, 1.0f},
      {0.0f, 0.8f, 0.5f, 1.0f},
  };
  uint8_t vdata[3 * VSIZE];
  memset(vdata, 0, sizeof(vdata));
  for (int i = 0; i < 3; i++) {
    memcpy(vdata + i * VSIZE, verts[i], 16);
    vdata[i * VSIZE + 16] = 0;
    vdata[i * VSIZE + 17] = 255; /* G */
    vdata[i * VSIZE + 18] = 0;
    vdata[i * VSIZE + 19] = 255; /* A */
  }

  struct pipe_resource vb_templ;
  memset(&vb_templ, 0, sizeof(vb_templ));
  vb_templ.target = PIPE_BUFFER;
  vb_templ.format = PIPE_FORMAT_R8_UNORM;
  vb_templ.width0 = sizeof(vdata);
  vb_templ.height0 = 1;
  vb_templ.depth0 = 1;
  vb_templ.array_size = 1;
  vb_templ.bind = PIPE_BIND_VERTEX_BUFFER;
  vb_templ.usage = PIPE_USAGE_DEFAULT;
  struct pipe_resource* vbuf = screen->resource_create(screen, &vb_templ);
  printf("vertex buffer = %p %s\n", (void*)vbuf, vbuf ? "OK" : "FAIL");
  if (vbuf == NULL) {
    fail++;
    goto out_res;
  }
  ctx->buffer_subdata(ctx, vbuf, 0 /* usage */, 0 /* offset */,
                      sizeof(vdata), vdata);

  struct pipe_vertex_buffer vb;
  memset(&vb, 0, sizeof(vb));
  vb.is_user_buffer = false;
  vb.buffer_offset = 0;
  vb.buffer.resource = vbuf;
  ctx->set_vertex_buffers(ctx, 1, &vb);

  struct pipe_vertex_element ve[2];
  memset(ve, 0, sizeof(ve));
  ve[0].src_offset = 0;
  ve[0].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
  ve[0].vertex_buffer_index = 0;
  ve[0].src_stride = VSIZE;
  ve[1].src_offset = 16;
  ve[1].src_format = PIPE_FORMAT_R8G8B8A8_UNORM;
  ve[1].vertex_buffer_index = 0;
  ve[1].src_stride = VSIZE;
  void* ve_state = ctx->create_vertex_elements_state(ctx, 2, ve);
  printf("vertex elements = %p %s\n", ve_state, ve_state ? "OK" : "FAIL");
  if (ve_state == NULL) {
    fail++;
    goto out_vbuf;
  }
  ctx->bind_vertex_elements_state(ctx, ve_state);

  /* ---- shader：position+color 直通 VS，color 直通 FS ---- */
  const enum tgsi_semantic sem_names[2] = {TGSI_SEMANTIC_POSITION,
                                           TGSI_SEMANTIC_COLOR};
  const unsigned sem_indices[2] = {0, 0};
  void* vs = util_make_vertex_passthrough_shader(ctx, 2, sem_names,
                                                 sem_indices, false);
  void* fs = util_make_fragment_passthrough_shader(
      ctx, TGSI_SEMANTIC_COLOR, TGSI_INTERPOLATE_PERSPECTIVE, false);
  printf("vs = %p %s, fs = %p %s\n", vs, vs ? "OK" : "FAIL", fs,
         fs ? "OK" : "FAIL");
  if (vs == NULL || fs == NULL) {
    fail++;
    goto out_ve;
  }
  ctx->bind_vs_state(ctx, vs);
  ctx->bind_fs_state(ctx, fs);

  /* ---- 固定功能状态 ---- */
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
  dsa.depth_enabled = 0;
  dsa.depth_func = PIPE_FUNC_ALWAYS;
  dsa.stencil[0].enabled = 0;
  dsa.stencil[1].enabled = 0;
  void* dsa_state = ctx->create_depth_stencil_alpha_state(ctx, &dsa);
  ctx->bind_depth_stencil_alpha_state(ctx, dsa_state);

  struct pipe_blend_state blend;
  memset(&blend, 0, sizeof(blend));
  /* colormask 默认 0 = 一个颜色通道都不写；clear 不走 blend，所以只有
   * draw 会“成功提交但没像素”。照 u_blitter 的默认值写满 RGBA。 */
  blend.rt[0].colormask = PIPE_MASK_RGBA;
  void* blend_state = ctx->create_blend_state(ctx, &blend);
  ctx->bind_blend_state(ctx, blend_state);

  struct pipe_viewport_state vp;
  memset(&vp, 0, sizeof(vp));
  vp.scale[0] = W / 2.0f;
  vp.scale[1] = H / 2.0f;
  vp.scale[2] = 1.0f;
  vp.translate[0] = W / 2.0f;
  vp.translate[1] = H / 2.0f;
  vp.translate[2] = 0.0f;
  ctx->set_viewport_states(ctx, 0, 1, &vp);

  struct pipe_scissor_state scissor;
  scissor.minx = 0;
  scissor.miny = 0;
  scissor.maxx = W;
  scissor.maxy = H;
  ctx->set_scissor_states(ctx, 0, 1, &scissor);

  /* ---- 先清黑，再画三角形 ---- */
  union pipe_color_union blue;
  memset(&blue, 0, sizeof(blue));
  blue.f[2] = 1.0f; /* blue background */
  blue.f[3] = 1.0f;
  ctx->clear(ctx, PIPE_CLEAR_COLOR0, 0xFu, 0u, NULL, &blue, 0.0, 0);
  printf("draw: green triangle\n");
  util_draw_arrays(ctx, MESA_PRIM_TRIANGLES, 0, 3);
  ctx->flush(ctx, NULL, 0);

  if (direct) {
    /* 直扫模式：屏幕 framebuffer 就是 render target，读 /dev/fb 验证 */
    int fbfd = open("/dev/fb", O_RDWR);
    if (fbfd < 0) {
      printf("direct: /dev/fb open failed\n");
      fail++;
    } else {
      size_t len = (size_t)fb_w * fb_h * 2u;
      void* base = mmap(NULL, len, PROT_READ, MAP_SHARED, fbfd, 0);
      if (base == MAP_FAILED) {
        printf("direct: /dev/fb mmap failed\n");
        fail++;
      } else {
        foot_cache_invalidate(base, (char*)base + len);
        uint16_t* p = (uint16_t*)base;
        uint32_t sp = fb_w;
        uint16_t c = p[300u * sp + 320u];
        uint16_t corner = p[10u * sp + 10u];
        printf("direct readback: center=0x%04x corner=0x%04x\n", c, corner);
        if (corner != 0x001Fu) {
          printf("direct FAIL: corner should be 0x001f, got 0x%04x\n", corner);
          fail++;
        }
        if (c != 0x07E0u) {
          printf("direct FAIL: center should be 0x07e0, got 0x%04x\n", c);
          fail++;
        }
        munmap(base, len);
      }
      close(fbfd);
    }
    printf("present(direct) zero-copy scanout\n");
  } else {
  /* ---- 读回 ---- */
  void* cpu = foot_v3d_bo_cpu_va(vr->bo->handle);
  if (cpu == NULL) {
    printf("foot_v3d_bo_cpu_va(%u) = NULL\n", vr->bo->handle);
    fail++;
  } else {
    foot_cache_invalidate(cpu, (char*)cpu + vr->bo->size);
    uint16_t* px = (uint16_t*)cpu;
    uint32_t stride_px = vr->slices[0].stride / 2u;
    uint16_t c = px[300u * stride_px + 320u];
    uint16_t corner = px[10u * stride_px + 10u];
    printf("readback: center=0x%04x corner=0x%04x\n", c, corner);

    /* 全图扫一遍：统计非蓝色像素，打印包围盒，判断三角形到底画到哪 */
    uint32_t count = 0, minx = W, miny = H, maxx = 0, maxy = 0;
    for (uint32_t y = 0; y < H; y++) {
      for (uint32_t x = 0; x < W; x++) {
        uint16_t p = px[y * stride_px + x];
        if (p != 0x001Fu) {
          count++;
          if (x < minx) minx = x;
          if (y < miny) miny = y;
          if (x > maxx) maxx = x;
          if (y > maxy) maxy = y;
        }
      }
    }
    printf("scan: non-blue=%u bbox=[%u,%u..%u,%u]\n", count, minx, miny,
           maxx, maxy);

    if (corner != 0x001Fu) {
      printf("readback FAIL: corner should be clear color 0x001f, got 0x%04x\n",
             corner);
      fail++;
    }
    if (c != 0x07E0u) {
      printf("readback FAIL: expect center 0x07e0 (green), got 0x%04x\n", c);
      fail++;
    }

    /* 【真正的上屏路径】离屏 BGR565 BO → TFU → 屏幕左上角。
     * 同格式直拷，不需要 64KB 中转区，也不经过会卡的 v3d_g2d_present。 */
    gpu_submit_tfu_req tr;
    memset(&tr, 0, sizeof(tr));
    tr.src_handle = vr->bo->handle;
    tr.dst_handle = 0; /* 屏幕 */
    tr.w = W;
    tr.h = H;
    tr.src_fmt = 6; /* RGB565 */
    tr.dst_fmt = 6;
    long prc = gc ? client_call_ex(gc, (void*)GPU_LOW_SUBMIT_TFU,
                                   &tr, sizeof(tr), 10000) : -1;
    printf("present(TFU) rc=%ld %s\n", prc, prc == 0 ? "OK" : "FAIL");
    if (prc != 0) {
      fail++;
    }
  }

  }

out_ve:
  ctx->delete_vertex_elements_state(ctx, ve_state);
  ctx->delete_vs_state(ctx, vs);
  ctx->delete_fs_state(ctx, fs);
  ctx->delete_rasterizer_state(ctx, rs_state);
  ctx->delete_depth_stencil_alpha_state(ctx, dsa_state);
  ctx->delete_blend_state(ctx, blend_state);
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
