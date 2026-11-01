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
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "client.h"

/* 与服务端 foot/driver/gpu/gpu_backend.h 的方法号保持一致 */
#define GPU_FN_QUERY 0
#define GPU_FN_FILL 1
#define GPU_FN_BLIT 2
#define GPU_FN_SYNC 3
#define GPU_FN_ALPHA 4
#define GPU_FN_PRESENT 5

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

/* 与 gpu_backend.h 的 gpu_blit_req / gpu_alpha_req / gpu_present_req 一致 */
typedef struct {
  unsigned int src;
  unsigned int dst;
  unsigned int sx, sy;
  unsigned int dx, dy;
  unsigned int w, h;
  unsigned int flags;
} gpu_blit_req;

typedef struct {
  unsigned int src;
  unsigned int dst;
  unsigned int sx, sy;
  unsigned int dx, dy;
  unsigned int w, h;
  unsigned int alpha; /* 0..256 */
  unsigned int flags;
} gpu_alpha_req;

typedef struct {
  unsigned int src;
  unsigned int sx, sy;
  unsigned int dx, dy;
  unsigned int w, h;
  unsigned int src_fmt;
  unsigned int flags;
} gpu_present_req;

/* BO 测试用：128x128 RGB565 = 32KB（foot_cap_alloc_frame 上限 256KB） */
#define BO_VA 0x50000000UL
#define GB_SRC_VA 0x51000000UL
#define GB_DST_VA 0x52000000UL
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

  /* 【先看是否已有 gpud 在跑】
   *
   * 之前每次都无条件 fork 一个 gpud、跑完 kill -9，会留下两个问题：
   *   1. 上一个 gpud 被硬杀后 V3D 硬件状态没清理，下次启动时
   *      `v3d: sms ... ree_cs=0x80000000`（正常应为 0x00000000）；
   *   2. 如果 kill 没生效，两个 gpud 会同时抢 V3D 硬件。
   * 两者都表现为 client_get 拿不到服务、随后崩在 libfoot 里。
   *
   * 所以先探测一次：已经有服务就直接复用，只有没有时才 fork。
   */
  pid_t pid = -1;
  /* 【TESTGPU_FRESH=1 强制重启 gpud】
   * 复用的好处是快，但改了 gpud 之后必须让它重启才能加载新内核 ——
   * 否则会一直测到旧版本（踩过：present 内核改了三轮，实际跑的一直是
   * 板子上那个老 gpud）。
   * 用环境变量而不是命令行参数，保持调用方式不变。 */
  int force_fresh = (getenv("TESTGPU_FRESH") != NULL);
  client_t* c = force_fresh ? NULL : client_get("gpu");
  if (force_fresh) {
    printf("TESTGPU_FRESH=1 -> 启动新的 gpud\n");
    fflush(stdout);
  } else if (c != NULL) {
    printf("reuse existing gpud (要换新版本请设 TESTGPU_FRESH=1)\n");
    fflush(stdout);
  }
  if (c == NULL) {
    /* 子进程：exec 服务端（/bin/gpud 常驻，client_run 阻塞） */
    pid = fork();
    if (pid == 0) {
      char* const argv[] = {"/bin/gpud", NULL};
      execv("/bin/gpud", argv);
      printf("exec /bin/gpud failed\n");
      fflush(stdout);
      _exit(1);
    }
  }

  /* 等 gpud 把服务注册上来（它要先做 SMS/PM/MMU/屏幕初始化）。
   * 【退避】前几次 20ms 快探（正常 1~2 次就起来），之后 1.5 倍递增到
   * 200ms 封顶。固定 100ms 会让 gpud 启动失败时刷屏 —— 每次失败的
   * client_get 都要打两行日志（"client get gpu error" + "get is null"），
   * 80 次就是 160 行噪声。总预算约 7 秒。 */
  int delay_ms = 20;
  for (int i = 0; c == NULL && i < 40; i++) {
    usleep(delay_ms * 1000);
    c = client_get("gpu");
    if (c != NULL) {
      break;
    }
    if (delay_ms < 200) {
      delay_ms = delay_ms * 3 / 2;
    }
    if (i == 12) {
      printf("still waiting for gpud (%.1fs)...\n", 1.4);
      fflush(stdout);
    }
  }
  if (c == NULL) {
    printf("gpud did not come up in ~7s "
           "(check /bin/gpud output for init failure)\n");
    fflush(stdout);
  }
  CHECK(c != NULL, "gpud service up");
  if (c == NULL) {
    printf("pass=%d fail=%d\nSummary:\n", g_pass, g_fail);
    if (pid > 0) {
      kill(pid, 15); /* SIGTERM：让 gpud 自己复位 V3D 再退出 */
      usleep(300 * 1000);
    }
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
    /* 【必须用 arena，不能用 frame】cap_frame 逐页分配后要求【物理恰好连续】
     * （cap_frame.c:159 的 "frame: not contiguous" 检查），页池碎片化后
     * 32KB 经常拿不到连续页而失败。OBJ_ARENA 走 phy_alloc_aligment，一次
     * 拿物理连续块 —— 这也正是 GPU BO 的语义要求（v3d_bo_create 按
     * pa + i*4K 逐页映射，物理必须连续）。 */
    foot_cap_t cap = foot_cap_alloc_arena(bo_bytes, 0, 0, &pa, &size);
    printf("alloc BO arena: %lu bytes -> cap=%lx pa=0x%08x size=%u\n", bo_bytes,
           (unsigned long)cap, pa, size);
    CHECK(cap != FOOT_CAP_NONE, "alloc BO arena");

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

  /* ---- 4) BLIT / ALPHA / PRESENT（GPU 计算内核，端到端）----
   * 这三条走的是自产 QPU 内核（copy_vec / alpha_vec / present_vec），
   * 数据格式是 ARGB8888（4 字节/像素），和上面的 RGB565 棋盘不同，
   * 所以另外分配两块 BO。
   * 尺寸取 256x256：宽度是 16 的倍数（blit/alpha 要求）也是 32 的倍数
   * （present 要求）。 */
  {
    /* 尺寸选 128x128：
     *   - 128 是 16 的倍数（blit/alpha 要求）也是 32 的倍数（present 要求）
     *   - 128*128*4 + 8192 = 72KB，远低于 foot_cap_alloc_arena 的上限。
     *     之前用 256x256 申请 264KB 直接越限，alloc 内部崩在 0x20（空指针）。 */
    const unsigned int W = 128, H = 128;
    const unsigned int bytes = W * H * 4u;
    unsigned int spa = 0, ssize = 0, dpa = 0, dsize = 0;
    printf("DBG: 申请 %u 字节 x2 (W=%u H=%u)\n", bytes + 8192u, W, H);
    foot_cap_t scap = foot_cap_alloc_arena(bytes + 8192u, 0, 0, &spa, &ssize);
    printf("DBG: src alloc cap=0x%lx pa=0x%x size=%u\n",
           (unsigned long)scap, spa, ssize);
    foot_cap_t dcap = foot_cap_alloc_arena(bytes + 8192u, 0, 0, &dpa, &dsize);
    printf("DBG: dst alloc cap=0x%lx pa=0x%x size=%u\n",
           (unsigned long)dcap, dpa, dsize);
    CHECK(scap != FOOT_CAP_NONE && dcap != FOOT_CAP_NONE, "alloc ARGB BOs");

    int m1 = -1, m2 = -1;
    if (scap != FOOT_CAP_NONE && dcap != FOOT_CAP_NONE) {
      m1 = foot_cap_map(scap, (void*)GB_SRC_VA, 0, ssize);
      printf("DBG: map src -> rc=%d\n", m1);
      m2 = foot_cap_map(dcap, (void*)GB_DST_VA, 0, dsize);
      printf("DBG: map dst -> rc=%d\n", m2);
    }

    if (scap != FOOT_CAP_NONE && dcap != FOOT_CAP_NONE && m1 == 0 && m2 == 0) {
      volatile unsigned int* s32 = (volatile unsigned int*)(uintptr_t)GB_SRC_VA;
      volatile unsigned int* d32 = (volatile unsigned int*)(uintptr_t)GB_DST_VA;

      /* 源：逐像素唯一（读错一个位置立刻现形） */
      for (unsigned int i = 0; i < W * H; i++) {
        s32[i] = 0x80000000u | i;
      }
      foot_cache_clean((void*)s32, (void*)(GB_SRC_VA + ssize));

      gpu_bo_import_req imp;
      memset(&imp, 0, sizeof(imp));
      imp.pa = spa;
      imp.size = ssize;
      long hs = (long)client_call_ex(c, (void*)GPU_LOW_BO_CREATE, &imp,
                                     sizeof(imp), 1000);
      imp.pa = dpa;
      imp.size = dsize;
      long hd = (long)client_call_ex(c, (void*)GPU_LOW_BO_CREATE, &imp,
                                     sizeof(imp), 1000);
      CHECK(hs > 0 && hd > 0, "BO create (ARGB x2)");

      if (hs > 0 && hd > 0) {
        /* ---- BLIT：dst = src ---- */
        for (unsigned int i = 0; i < W * H; i++) {
          d32[i] = 0;
        }
        foot_cache_clean((void*)d32, (void*)(GB_DST_VA + dsize));

        gpu_blit_req br;
        memset(&br, 0, sizeof(br));
        br.src = (unsigned int)hs;
        br.dst = (unsigned int)hd;
        br.w = W;
        br.h = H;
        long rc = (long)client_call_ex(c, (void*)GPU_FN_BLIT, &br,
                                       sizeof(br), 5000);
        foot_cache_invalidate((void*)d32, (void*)(GB_DST_VA + dsize));
        unsigned int match = 0;
        for (unsigned int i = 0; i < W * H; i++) {
          if (d32[i] == s32[i]) {
            match++;
          }
        }
        printf("BLIT: rc=%ld match=%u/%u px0=%08x\n", rc, match, W * H, d32[0]);
        CHECK(rc == 0 && match == W * H, "gpu blit 逐像素正确");

        /* ---- ALPHA：dst = src*128 + dst*(256-128) 逐通道 ---- */
        for (unsigned int i = 0; i < W * H; i++) {
          d32[i] = 0xFFFF0000u; /* 纯红背景 */
        }
        foot_cache_clean((void*)d32, (void*)(GB_DST_VA + dsize));

        gpu_alpha_req ar;
        memset(&ar, 0, sizeof(ar));
        ar.src = (unsigned int)hs;
        ar.dst = (unsigned int)hd;
        ar.w = W;
        ar.h = H;
        ar.alpha = 128;
        rc = (long)client_call_ex(c, (void*)GPU_FN_ALPHA, &ar, sizeof(ar), 5000);
        foot_cache_invalidate((void*)d32, (void*)(GB_DST_VA + dsize));
        /* src[0]=0x80000000(A=80 R=0 G=0 B=0) + dst=0xFFFF0000(A=FF R=FF)
         *   A=(0x80+0xFF)*128>>8=0xBF  R=(0x00+0xFF)*128>>8=0x7F  G=B=0 */
        printf("ALPHA: rc=%ld px0=%08x (expect bf7f0000)\n", rc, d32[0]);
        CHECK(rc == 0 && d32[0] == 0xBF7F0000u, "gpu alpha 混合正确");

        /* ---- PRESENT：ARGB8888 -> 屏幕 RGB565 ---- */
        gpu_present_req pr;
        memset(&pr, 0, sizeof(pr));
        pr.src = (unsigned int)hd;
        pr.w = W;
        pr.h = H;
        rc = (long)client_call_ex(c, (void*)GPU_FN_PRESENT, &pr, sizeof(pr),
                                  5000);
        printf("PRESENT: rc=%ld (%ux%u -> 屏幕 0,0)\n", rc, W, H);
        CHECK(rc == 0, "gpu present 上屏");

        gpu_bo_ref ref;
        ref.handle = (unsigned int)hd;
        client_call_ex(c, (void*)GPU_LOW_BO_DESTROY, &ref, sizeof(ref), 1000);
        ref.handle = (unsigned int)hs;
        client_call_ex(c, (void*)GPU_LOW_BO_DESTROY, &ref, sizeof(ref), 1000);
      }
    }
  }

  /* ---- 5) SYNC ---- */
  CHECK((long)client_call_ex(c, (void*)GPU_FN_SYNC, NULL, 0, 1000) == 0,
        "gpu sync");

  clirent_un_regist(c);
  printf("pass=%d fail=%d\n", g_pass, g_fail);
  printf("\nSummary:\n");

  /* 只杀自己 fork 的那个；复用已有 gpud 时不能杀它——
   * 它是常驻服务，杀了会留下脏的硬件状态影响下一次运行。
   *
   * 【必须用 SIGTERM 而不是 SIGKILL】gpud 装了 SIGTERM 处理器，
   * 会在退出前把 V3D 硬件置回复位态（清 PM.V3DRSTN）。
   * 用 kill -9 时处理器跑不到，硬件停在运行态，下次启动就会看到
   *   v3d: ... ree_cs=0x80000000      （正常应为 0x00000000）
   * 而那个脏状态会让下一次 gpud 起来的服务无效。 */
  if (pid > 0) {
    kill(pid, 15); /* SIGTERM */
    usleep(300 * 1000); /* 等它复位硬件并退出 */
  }
  return g_fail > 0 ? 1 : 0;
}
