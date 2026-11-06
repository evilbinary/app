/* test-mesa-clear.c - 用 Mesa v3d gallium 驱动做一次真实 clear（路线 A）
 *
 * 链路：
 *   pipe_screen / pipe_context
 *     → Mesa v3d 生成 BCL/RCL
 *     → libdrm 替身 DRM_IOCTL_V3D_SUBMIT_CL
 *     → gpud v3d_3d_submit_cl
 *     → V3D 硬件
 *
 * 验收：
 *   1) CL 提交后 RFC 递增、CT1CS 清零（gpud 日志）
 *   2) 线性 ARGB8888 BO 的 (0,0)/(320,240) 变成 clear 色
 *   3) 可选：GPU_LOW_PRESENT 把 BO 贴到屏幕左上角
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#include "client.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "util/u_inlines.h"
#include "util/xmlconfig.h"
#include "v3d_drm_public.h"
#include "broadcom/common/v3d_limits.h"
#include "v3d/v3d_screen.h"
#include "v3d/v3d_resource.h"
#include "v3d/v3d_bufmgr.h"

#define GPU_FN_PRESENT 5
typedef struct {
  unsigned int src, sx, sy, dx, dy, w, h, src_fmt, flags;
} gpu_present_req;

extern void* foot_gpu_bo_cpu_va(unsigned int handle);

/* ---- BO 的 CPU 映射（libdrm 的 CREATE_BO 回调）---- */
#define BO_VA_BASE 0x52000000UL
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
  printf("bridge_bo_alloc: size=%u pa=0x%08x va=0x%08x\n", size, p,
         (unsigned int)va);
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
  if (fd < 0) {
    printf("open /dev/null failed\n");
    return NULL;
  }
  struct pipe_screen* screen = v3d_drm_screen_create(fd, &config);
  if (screen == NULL) {
    close(fd);
    return NULL;
  }
  *fd_out = fd;
  return screen;
}

int main(void) {
  const unsigned int W = 640, H = 480;
  int fd = -1;
  int fail = 0;

  /* devd 前台管道 + 块缓冲会看不到进度；无缓冲便于定位卡点。 */
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("=== test-mesa-clear (Mesa v3d gallium clear) ===\n");

  struct pipe_screen* screen = make_screen(&fd);
  printf("screen = %p %s\n", (void*)screen, screen ? "OK" : "FAIL");
  if (screen == NULL) {
    return 1;
  }
  struct v3d_screen* vs = v3d_screen(screen);
  printf("devinfo: ver=%u rev=%u compat=%u qpu=%u vpm=%u\n",
         vs->devinfo.ver, vs->devinfo.rev, vs->devinfo.compat_rev,
         vs->devinfo.qpu_count, vs->devinfo.vpm_size);

  struct pipe_context* ctx = screen->context_create(screen, NULL, 0);
  printf("context = %p %s\n", (void*)ctx, ctx ? "OK" : "FAIL");
  if (ctx == NULL) {
    screen->destroy(screen);
    close(fd);
    return 1;
  }

  struct pipe_resource templ;
  memset(&templ, 0, sizeof(templ));
  templ.target = PIPE_TEXTURE_2D;
  templ.format = PIPE_FORMAT_B8G8R8A8_UNORM;
  templ.width0 = W;
  templ.height0 = H;
  templ.depth0 = 1;
  templ.array_size = 1;
  templ.last_level = 0;
  templ.bind = PIPE_BIND_RENDER_TARGET | PIPE_BIND_LINEAR;
  templ.usage = PIPE_USAGE_DEFAULT;
  struct pipe_resource* res = screen->resource_create(screen, &templ);
  printf("resource = %p %s\n", (void*)res, res ? "OK" : "FAIL");
  if (res == NULL) {
    fail++;
    goto out_ctx;
  }

  struct v3d_resource* vr = v3d_resource(res);
  printf("resource: bo=%p handle=%u size=%u tiled=%d cpp=%d stride=%u "
         "tiling=%d\n",
         (void*)vr->bo, vr->bo ? vr->bo->handle : 0, vr->bo ? vr->bo->size : 0,
         vr->tiled, vr->cpp, vr->slices[0].stride, vr->slices[0].tiling);

  struct pipe_framebuffer_state fb;
  memset(&fb, 0, sizeof(fb));
  fb.width = W;
  fb.height = H;
  fb.nr_cbufs = 1;
  memset(&fb.cbufs[0], 0, sizeof(fb.cbufs[0]));
  fb.cbufs[0].format = res->format;
  fb.cbufs[0].texture = res;
  fb.cbufs[0].first_layer = 0;
  fb.cbufs[0].last_layer = 0;
  fb.cbufs[0].level = 0;
  ctx->set_framebuffer_state(ctx, &fb);

  union pipe_color_union color;
  memset(&color, 0, sizeof(color));
  color.f[0] = 1.0f; /* red */
  color.f[1] = 0.0f;
  color.f[2] = 0.0f;
  color.f[3] = 1.0f;

  printf("clear: red 640x480\n");
  ctx->clear(ctx, PIPE_CLEAR_COLOR0, 0xFu /* 4 bits per RT0 */, 0u, NULL,
             &color, 0.0, 0);
  ctx->flush(ctx, NULL, 0);

  /* GPU 写完后，CPU 侧必须失效 cache 才能看到 DRAM 里的结果 */
  void* cpu = foot_gpu_bo_cpu_va(vr->bo->handle);
  if (cpu == NULL) {
    printf("foot_gpu_bo_cpu_va(%u) = NULL\n", vr->bo->handle);
    fail++;
  } else {
    foot_cache_invalidate(cpu, (char*)cpu + vr->bo->size);
    uint32_t* px = (uint32_t*)cpu;
    uint32_t stride_px = vr->slices[0].stride / 4u;
    if (stride_px == 0u) {
      stride_px = W;
    }
    printf("readback: (0,0)=0x%08x (320,240)=0x%08x (639,479)=0x%08x\n",
           px[0], px[240u * stride_px + 320u],
           px[479u * stride_px + 639u]);
    if (px[0] != 0xFFFF0000u) {
      printf("readback FAIL: expect 0xffff0000 (ARGB red)\n");
      fail++;
    }

    /* 把 BO 贴到屏幕左上角，肉眼确认。
     * 【暂时关闭】v3d_g2d_present_gpu() 的 64KB scratch 放不下
     * 640x480 ARGB→RGB565（需要 614KB），小尺寸试的时候 present 路径
     * 还会把进程卡死、拖死单线程 devd。A 的验收以 BO 读回为准。 */
#if 0
    gpu_present_req pr;
    memset(&pr, 0, sizeof(pr));
    pr.src = vr->bo->handle;
    pr.w = W;
    pr.h = H;
    client_t* gc = client_get("gpu");
    long prc = gc ? client_call_ex(gc, (void*)GPU_FN_PRESENT, &pr, sizeof(pr),
                                   5000) : -1;
    printf("present rc=%ld %s\n", prc, prc == 0 ? "OK" : "FAIL");
#endif
  }

  pipe_resource_reference(&res, NULL);
out_ctx:
  ctx->destroy(ctx);
  screen->destroy(screen);
  close(fd);
  printf("=== %s (fail=%d) ===\n", fail == 0 ? "PASS" : "FAIL", fail);
  return fail;
}
