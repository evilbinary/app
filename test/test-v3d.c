/*******************************************************************
 * v3dd（V3D GPU 服务）客户端回归测试。
 *
 * 自包含：fork 一个子进程 exec /bin/v3dd 启动服务端，父进程当客户端
 * 调用 INFO / IDENT / FILL（四象限填色），跑完杀掉服务端。
 *
 * 跑法（设备端）：/bin/test-v3d
 *   主机侧：python3 tools/ltp_batch.py --timeout 90 /bin/test-v3d
 ********************************************************************/
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "client.h"

/* 与服务端 v3dd.c 的协议一致 */
#define V3D_FN_IDENT 0
#define V3D_FN_FILL 1
#define V3D_FN_INFO 2

typedef struct {
  unsigned int x, y, w, h;
  unsigned int color;
} v3d_fill_req_t;

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
  printf("=== v3d client test ===\n");
  /* 【关键】fork 前必须清空 stdio 缓冲：否则子进程会带着半截缓冲去
   * exec，失败后 _exit 再 flush 会踩到已失效的状态（实测 fault 0x34）。 */
  fflush(stdout);

  /* 子进程：exec 服务端（/bin/v3dd 常驻，client_run 阻塞）。
   * 用 execv（数组形式），避免 execl 的 varargs 在 fork 子进程里出问题。 */
  pid_t pid = fork();
  if (pid == 0) {
    char* const argv[] = {"/bin/v3dd", NULL};
    execv("/bin/v3dd", argv);
    /* exec 失败才会到这里 */
    printf("exec /bin/v3dd failed\n");
    fflush(stdout);
    _exit(1);
  }

  /* 等 v3dd 把服务注册上来（它要先做 SMS/PM/MMU/屏幕初始化） */
  client_t* c = NULL;
  for (int i = 0; i < 50; i++) {
    usleep(100000); /* 100ms */
    c = client_get("v3d");
    if (c != NULL) break;
  }
  CHECK(c != NULL, "v3dd service up");
  if (c == NULL) {
    printf("pass=%d fail=%d\nSummary:\n", g_pass, g_fail);
    return 1;
  }

  /* INFO：返回 (width << 16) | height */
  void* info = client_call_ex(c, (void*)V3D_FN_INFO, NULL, 0, 1000);
  unsigned int w = ((unsigned long)info >> 16) & 0xFFFFu;
  unsigned int h = (unsigned long)info & 0xFFFFu;
  printf("v3d info: %ux%u\n", w, h);
  CHECK(w > 0 && h > 0, "screen info");

  /* IDENT：HUB_IDENT0 应为 "VHUB" */
  void* ident = client_call_ex(c, (void*)V3D_FN_IDENT, NULL, 0, 1000);
  printf("v3d ident: 0x%08lx\n", (unsigned long)ident);
  CHECK(((unsigned long)ident & 0xFFFFFFFFu) == 0x42554856u, "ident == VHUB");

  if (w > 0 && h > 0) {
    v3d_fill_req_t r;
    memset(&r, 0, sizeof(r));
    r.w = w / 2;
    r.h = h / 2;

    r.x = 0;
    r.y = 0;
    r.color = 0xF800; /* 红 */
    CHECK((long)client_call_ex(c, (void*)V3D_FN_FILL, &r, sizeof(r), 2000) == 0,
          "fill top-left red");

    r.x = w / 2;
    r.color = 0x07E0; /* 绿 */
    CHECK((long)client_call_ex(c, (void*)V3D_FN_FILL, &r, sizeof(r), 2000) == 0,
          "fill top-right green");

    r.x = 0;
    r.y = h / 2;
    r.color = 0x001F; /* 蓝 */
    CHECK((long)client_call_ex(c, (void*)V3D_FN_FILL, &r, sizeof(r), 2000) == 0,
          "fill bottom-left blue");

    r.x = w / 2;
    r.color = 0xFFFF; /* 白 */
    CHECK((long)client_call_ex(c, (void*)V3D_FN_FILL, &r, sizeof(r), 2000) == 0,
          "fill bottom-right white");
  }

  clirent_un_regist(c);
  printf("pass=%d fail=%d\n", g_pass, g_fail);
  printf("\nSummary:\n");

  /* SIGKILL 收服务端子进程（sys_kill 只支持 SIGKILL） */
  kill(pid, 9);
  return g_fail > 0 ? 1 : 0;
}
