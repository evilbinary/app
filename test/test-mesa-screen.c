/* test-mesa-screen.c - Mesa v3d gallium screen 创建验证（路线 A 的第一步）
 *
 * 目标：让 libmesa 的 v3d gallium 驱动通过 libdrm 替身完成
 *   v3d_drm_screen_create()
 * 的初始化链：GET_PARAM → devinfo → pipe_screen。
 *
 * 这一步不提交任何 GPU job，只验证：
 *   1) libdrm 替身能回答 Mesa 需要的 V3D_CORE0_IDENT0/1、HUB_IDENT3
 *   2) pipe_screen_config 的最小 driconf cache 不会崩
 *   3) bridge_bo_alloc() 的 CPU 映射回调能编进来
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#include "client.h"
#include "pipe/p_screen.h"
#include "util/xmlconfig.h"
#include "v3d_drm_public.h"
#include "v3d/v3d_screen.h"

/* ---- BO 的 CPU 映射（libdrm 的 DRM_IOCTL_V3D_CREATE_BO 会回调）---- */
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

int main(void) {
  printf("=== test-mesa-screen (v3d gallium screen) ===\n");

  /* 空 driconf cache：v3d_screen_create() 会调 driParseConfigFiles()，
   * 必须给 options/options_info 两个有效指针，否则会解 NULL。
   * tableSize=0 表示没有选项；driCheckOption() 查不到就返回 false。 */
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

  struct pipe_screen_config config;
  memset(&config, 0, sizeof(config));
  config.options = &options_cache;
  config.options_info = &info_cache;

  /* libdrm 替身忽略 fd；/dev/null 只是给 os_dupfd_cloexec() 一个合法 fd。 */
  int fd = open("/dev/null", O_RDWR);
  if (fd < 0) {
    printf("open /dev/null failed\n");
    return 1;
  }

  struct pipe_screen* screen = v3d_drm_screen_create(fd, &config);
  printf("v3d_drm_screen_create = %p %s\n", (void*)screen,
         screen ? "OK" : "FAIL");
  if (screen == NULL) {
    close(fd);
    return 1;
  }

  struct v3d_screen* vs = v3d_screen(screen);
  printf("devinfo: ver=%u rev=%u compat_rev=%u qpu=%u vpm=%u\n",
         vs->devinfo.ver, vs->devinfo.rev, vs->devinfo.compat_rev,
         vs->devinfo.qpu_count, vs->devinfo.vpm_size);
  printf("caps: csd=%d cache_flush=%d perfmon=%d\n", vs->has_csd,
         vs->has_cache_flush, vs->has_perfmon);

  screen->destroy(screen);
  close(fd);
  printf("=== PASS ===\n");
  return 0;
}
