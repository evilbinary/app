/*******************************************************************
 * GPU 服务（gpud）回归测试。
 *
 * 覆盖：
 *   1) 高层：QUERY（屏幕信息）+ FILL（四象限）
 *   2) 低层：BO_CREATE（导入 pa）→ SUBMIT_TFU（BO → 屏幕）→ BO_DESTROY
 *
 * 自包含：fork 一个子进程 exec /bin/gpud 启动服务端。
 *
 * 跑法（设备端）：/bin/test-gpu
 *   主机侧：python3 tools/ltp_batch.py --timeout 90 /bin/test-gpu
 ********************************************************************/
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "client.h"

/* 与服务端 foot/driver/gpu/gpu_backend.h 的方法号保持一致 */
#define GPU_FN_QUERY 0
#define GPU_FN_FILL 1
#define GPU_FN_SYNC 3

#define GPU_LOW_GET_BO_OFFSET 101
#define GPU_LOW_BO_CREATE 110
#define GPU_LOW_BO_DESTROY 112
#define GPU_LOW_SUBMIT_TFU 122

typedef struct {
  unsigned int x, y, w, h;
  unsigned int color;
} gpu_fill_req;

typedef struct {
  unsigned int pa, size, flags;
} gpu_bo_import_req;

typedef struct {
  unsigned int handle;
} gpu_bo_ref;

typedef struct {
  unsigned int src_handle, src_offset;
  unsigned int dst_handle, dst_offset;
  unsigned int w, h;
  unsigned int src_fmt, dst_fmt;
  unsigned int flags;
} gpu_submit_tfu_req;

/* BO 测试用：128x128 RGB565 = 32KB（foot_cap_alloc_frame 上限 256KB） */
#define BO_VA 0x50000000UL
#define BO_W 128
#define BO_H 128
#define BO_CPP 2 /* RGB565 */

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg)         \
  do {                           \
    if (cond) {                  \
      g_pass++;                  \
      printf("TPASS: %s\n", msg); \
    } else {                     \
      g_fail++;                  \
      printf("TFAIL: %s\n", msg); \
    }                            \
  } while (0)

int main(void) {
  setvbuf(stdout, NULL, _IOLBF, 0);
  printf("=== gpu client test ===\n");
  fflush(stdout);

  /* 子进程：exec 服务端（/bin/gpud 常驻，client_run 阻塞） */
  pid_t pid = fork();
  if (pid == 0) {
    char* const argv[] = {"/bin/gpud", NULL};
    execv("/bin/gpud", argv);
    printf("exec /bin/gpud failed\n");
    fflush(stdout);
    _exit(1);
  }

  /* 等 gpud 把服务注册上来（它要先做 SMS/PM/MMU/屏幕初始化） */
  client_t* c = NULL;
  for (int i = 0; i < 80; i++) {
    usleep(100000); /* 100ms */
    c = client_get("gpu");
    if (c != NULL) break;
  }
  CHECK(c != NULL, "gpud service up");
  if (c == NULL) {
    printf("pass=%d fail=%d\nSummary:\n", g_pass, g_fail);
    kill(pid, 9);
    return 1;
  }

  /* ---- 1) QUERY：(width << 16) | height ---- */
  unsigned int v = (unsigned int)(unsigned long)client_call_ex(
      c, (void*)GPU_FN_QUERY, NULL, 0, 1000);
  unsigned int w = v >> 16, h = v & 0xFFFFu;
  printf("gpu screen: %ux%u\n", w, h);
  CHECK(w > 0 && h > 0, "screen info");

  /* ---- 2) FILL 四象限 ---- */
  if (w > 0 && h > 0) {
    gpu_fill_req r;
    memset(&r, 0, sizeof(r));
    r.w = w / 2;
    r.h = h / 2;

    r.x = 0;
    r.y = 0;
    r.color = 0xF800;
    CHECK((long)client_call_ex(c, (void*)GPU_FN_FILL, &r, sizeof(r), 2000) == 0,
          "fill top-left red");

    r.x = w / 2;
    r.color = 0x07E0;
    CHECK((long)client_call_ex(c, (void*)GPU_FN_FILL, &r, sizeof(r), 2000) == 0,
          "fill top-right green");

    r.x = 0;
    r.y = h / 2;
    r.color = 0x001F;
    CHECK((long)client_call_ex(c, (void*)GPU_FN_FILL, &r, sizeof(r), 2000) == 0,
          "fill bottom-left blue");

    r.x = w / 2;
    r.color = 0xFFFF;
    CHECK((long)client_call_ex(c, (void*)GPU_FN_FILL, &r, sizeof(r), 2000) == 0,
          "fill bottom-right white");
  }

  /* ---- 3) BO + SUBMIT_TFU：BO → 屏幕左上角 ---- */
  {
    unsigned int pa = 0, size = 0;
    unsigned long bo_bytes = (unsigned long)BO_W * BO_H * BO_CPP;
    foot_cap_t cap = foot_cap_alloc_frame(bo_bytes, 0, 0, &pa, &size);
    printf("alloc BO frame: %lu bytes -> cap=%lx pa=0x%08x size=%u\n", bo_bytes,
           (unsigned long)cap, pa, size);
    CHECK(cap != FOOT_CAP_NONE, "alloc BO frame");

    if (cap != FOOT_CAP_NONE &&
        foot_cap_map(cap, (void*)BO_VA, 0, size) == 0) {
      /* 填充 16x16 棋盘（红/蓝），让上屏结果一眼可辨 */
      unsigned short* bo = (unsigned short*)BO_VA;
      for (int y = 0; y < BO_H; y++) {
        for (int x = 0; x < BO_W; x++) {
          bo[y * BO_W + x] = (((x >> 4) + (y >> 4)) & 1) ? 0xF800 : 0x001F;
        }
      }
      foot_cache_clean(bo, (void*)(BO_VA + size));

      /* 导入为 BO（gpud 映射进 GPU VA） */
      gpu_bo_import_req imp;
      memset(&imp, 0, sizeof(imp));
      imp.pa = pa;
      imp.size = size;
      imp.flags = 0;
      long handle =
          (long)client_call_ex(c, (void*)GPU_LOW_BO_CREATE, &imp, sizeof(imp), 1000);
      printf("BO create: pa=0x%08x size=%u handle=%ld\n", pa, size, handle);
      CHECK(handle > 0, "BO create");

      if (handle > 0) {
        gpu_bo_ref ref;
        ref.handle = (unsigned int)handle;
        long off = (long)client_call_ex(c, (void*)GPU_LOW_GET_BO_OFFSET, &ref,
                                        sizeof(ref), 1000);
        printf("BO offset(GPU VA) = 0x%08lx\n", (unsigned long)off);
        CHECK(off > 0, "BO offset");

        /* SUBMIT_TFU：BO → 屏幕左上角（同 RGB565 格式） */
        gpu_submit_tfu_req t;
        memset(&t, 0, sizeof(t));
        t.src_handle = (unsigned int)handle;
        t.src_offset = 0;
        t.dst_handle = 0; /* 屏幕 */
        t.dst_offset = 0; /* (0,0) */
        t.w = BO_W;
        t.h = BO_H;
        t.src_fmt = 1; /* GPU_FMT_RGB565 */
        t.dst_fmt = 1;
        long rc = (long)client_call_ex(c, (void*)GPU_LOW_SUBMIT_TFU, &t, sizeof(t),
                                       3000);
        printf("SUBMIT_TFU rc=%ld (BO -> screen 0,0 %dx%d)\n", rc, BO_W, BO_H);
        CHECK(rc == 0, "submit TFU (BO -> screen)");

        /* 释放 */
        long drc = (long)client_call_ex(c, (void*)GPU_LOW_BO_DESTROY, &ref,
                                        sizeof(ref), 1000);
        CHECK(drc == 0, "BO destroy");
      }
    }
  }

  /* ---- 4) SYNC ---- */
  CHECK((long)client_call_ex(c, (void*)GPU_FN_SYNC, NULL, 0, 1000) == 0,
        "gpu sync");

  clirent_un_regist(c);
  printf("pass=%d fail=%d\n", g_pass, g_fail);
  printf("\nSummary:\n");

  kill(pid, 9);
  return g_fail > 0 ? 1 : 0;
}
