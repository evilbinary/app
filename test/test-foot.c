/*******************************************************************
 * foot 服务框架回归测试
 *   1) 注册/发现（含未注册名字应失败）
 *   2) 单次往返 + 参数传递
 *   3) 连续调用（槽位是否每次归还）
 *   4) 调用超时（服务端慢于客户端超时）
 *   5) 超时后恢复（槽位没有被永久占死）
 *
 * 注意：返回值只能传"值"（整数/错误码），不能传指针——
 * 服务端与客户端是两个进程，指针不通用。数据请走共享 args 区或 CAP_MEM。
 *
 * 跑法（设备端）：/bin/test-foot
 *   主机侧：python3 tools/ltp_batch.py --timeout 60 /bin/test-foot
 ********************************************************************/
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "client.h"

enum { FN_ECHO = 1, FN_SLOW = 2 };

#define SERVER_SLOTS 4
#define SEQ_CALLS 32
#define RECOVER_CALLS (SERVER_SLOTS * 2)
#define SLOW_MS 1200 /* 服务端慢调用耗时，必须明显大于客户端超时 */
#define SLOW_TIMEOUT_MS 300

static int g_pass = 0;
static int g_fail = 0;

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

static void sleep_ms(long ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}

static void* serve(void* fn, void* args) {
  char* a = (char*)args;
  switch ((int)(long)fn) {
    case FN_ECHO:
      /* 把收到的首字节编码进返回值，验证参数确实到达服务端 */
      return (void*)(long)(0x1000 + (unsigned char)a[0]);
    case FN_SLOW:
      /* 故意慢于客户端超时 */
      sleep_ms(SLOW_MS);
      return (void*)0x1234;
    default:
      break;
  }
  return NULL;
}

int main(void) {
  static char svc_name[] = "foottest";
  static char bad_name[] = "foot_nosuch_service";
  pid_t pid;
  client_t* c;
  char a1[] = "A";
  void* r1;
  void* r3;
  int ok;
  int ok4;
  int i;

  /* 串口按行刷新：tools/ltp_batch.py 靠 "Summary:" 判定结束 */
  setvbuf(stdout, NULL, _IOLBF, 0);
  printf("=== foot test start ===\n");

  /* 0. 发现：未注册的名字必须失败 */
  {
    client_t* bad = client_get(bad_name);
    CHECK(bad == NULL, "get unknown service fails");
    if (bad != NULL) clirent_un_regist(bad);
  }

  /* 子进程：注册服务并进入服务循环 */
  pid = fork();
  if (pid == 0) {
    client_t* server = client_regist_ex(svc_name, SERVER_SLOTS);
    if (server == NULL) {
      printf("server regist failed\n");
      _exit(1);
    }
    client_run(server, serve);
    _exit(0);
  }

  /* 等服务端完成注册 */
  sleep_ms(1000);

  c = client_get(svc_name);
  CHECK(c != NULL, "client_get registered service");
  if (c == NULL) {
    kill(pid, SIGKILL);
    printf("pass=%d fail=%d\n", g_pass, g_fail);
    printf("\nSummary:\n");
    return 1;
  }
  CHECK(c->api_size == SERVER_SLOTS, "api_size == 4");

  /* 1. 单次往返 + 参数 */
  r1 = client_call_ex(c, (void*)FN_ECHO, a1, sizeof(a1), 1000);
  CHECK(r1 == (void*)(long)(0x1000 + 'A'), "echo roundtrip with arg");

  /* 2. 连续调用：验证槽位在每次调用后归还 */
  ok = 0;
  for (i = 0; i < SEQ_CALLS; i++) {
    char a[] = "Z";
    void* r = client_call_ex(c, (void*)FN_ECHO, a, sizeof(a), 1000);
    if (r == (void*)(long)(0x1000 + 'Z')) ok++;
  }
  CHECK(ok == SEQ_CALLS, "32 sequential calls");

  /* 3. 超时：客户端 300ms，服务端约 1.2s，期望 -1 */
  r3 = client_call_ex(c, (void*)FN_SLOW, NULL, 0, SLOW_TIMEOUT_MS);
  CHECK(r3 == (void*)-1, "slow call times out");

  /* 4. 超时后恢复：等慢调用跑完，所有槽位都应可再次使用 */
  sleep_ms(SLOW_MS + 800);
  ok4 = 0;
  for (i = 0; i < RECOVER_CALLS; i++) {
    char a[] = "B";
    void* r = client_call_ex(c, (void*)FN_ECHO, a, sizeof(a), 2000);
    if (r == (void*)(long)(0x1000 + 'B')) ok4++;
  }
  CHECK(ok4 == RECOVER_CALLS, "recover after timeout (all slots)");

  printf("pass=%d fail=%d\n", g_pass, g_fail);

  clirent_un_regist(c);
  kill(pid, SIGKILL);
  printf("\nSummary:\n");
  return g_fail > 0 ? 1 : 0;
}
