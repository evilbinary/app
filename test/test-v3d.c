/*******************************************************************
 * GPU 服务（gpud）客户端回归测试。
 *
 * 自包含：fork 一个子进程 exec /bin/gpud 启动服务端，父进程当客户端
 * 调 QUERY / FILL（四象限填色），跑完杀掉服务端。
 *
 * 跑法（设备端）：/bin/test-v3d
 *   主机侧：python3 tools/ltp_batch.py --timeout 90 /bin/test-v3d
 ********************************************************************/
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "client.h"

/* 与服务端 foot/driver/gpu/gpu_backend.h 的方法号保持一致 */
#define GPU_FN_QUERY 0
#define GPU_FN_FILL 1
#define GPU_FN_BLIT 2
#define GPU_FN_SYNC 3

typedef struct {
  unsigned int x, y, w, h; /* 目标矩形 */
  unsigned int color;      /* 颜色（RGB565 用低 16 位） */
} gpu_fill_req;

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

  /* QUERY：返回 (width << 16) | height */
  unsigned int v = (unsigned int)(unsigned long)client_call_ex(
      c, (void*)GPU_FN_QUERY, NULL, 0, 1000);
  unsigned int w = v >> 16, h = v & 0xFFFFu;
  printf("gpu screen: %ux%u\n", w, h);
  CHECK(w > 0 && h > 0, "screen info");

  if (w > 0 && h > 0) {
    gpu_fill_req r;
    memset(&r, 0, sizeof(r));
    r.w = w / 2;
    r.h = h / 2;

    r.x = 0;
    r.y = 0;
    r.color = 0xF800; /* 红（RGB565） */
    CHECK((long)client_call_ex(c, (void*)GPU_FN_FILL, &r, sizeof(r), 2000) == 0,
          "fill top-left red");

    r.x = w / 2;
    r.color = 0x07E0; /* 绿 */
    CHECK((long)client_call_ex(c, (void*)GPU_FN_FILL, &r, sizeof(r), 2000) == 0,
          "fill top-right green");

    r.x = 0;
    r.y = h / 2;
    r.color = 0x001F; /* 蓝 */
    CHECK((long)client_call_ex(c, (void*)GPU_FN_FILL, &r, sizeof(r), 2000) == 0,
          "fill bottom-left blue");

    r.x = w / 2;
    r.color = 0xFFFF; /* 白 */
    CHECK((long)client_call_ex(c, (void*)GPU_FN_FILL, &r, sizeof(r), 2000) == 0,
          "fill bottom-right white");
  }

  clirent_un_regist(c);
  printf("pass=%d fail=%d\n", g_pass, g_fail);
  printf("\nSummary:\n");

  kill(pid, 9);
  return g_fail > 0 ? 1 : 0;
}
