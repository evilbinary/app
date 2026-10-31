/*******************************************************************
 * foot 服务框架回归测试
 *   1) 注册/发现（含未注册名字应失败）
 *   2) 单次往返 + 参数传递
 *   3) payload 长度按 len 传递（200 字节 / 0 字节）
 *   4) 连续调用（槽位是否每次归还）
 *   5) 调用超时（服务端慢于客户端超时）
 *   6) 超时后恢复（槽位没有被永久占死）
 *
 * 注意：返回值只能传"值"（整数/错误码），不能传指针——
 * 服务端与客户端是两个进程，指针不通用。数据请走共享 args 区或 CAP_MEM。
 *
 * 跑法（设备端）：/bin/test-foot
 *   主机侧：python3 tools/ltp_batch.py --timeout 90 /bin/test-foot
 ********************************************************************/
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "client.h"

enum { FN_ECHO = 1, FN_LEN = 2, FN_SLOW = 3 };

#define SERVER_SLOTS 64 /* MAX_API_SLOTS：验证并发槽位上到 64 */
#define SEQ_CALLS 32
#define RECOVER_CALLS (SERVER_SLOTS / 8)
#define SLOW_MS 1200 /* 服务端慢调用耗时，必须明显大于客户端超时 */
#define SLOW_TIMEOUT_MS 300
#define BIG_LEN 200

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

static void* serve(void* fn, void* args, unsigned int len) {
  char* a = (char*)args;
  switch ((int)(long)fn) {
    case FN_ECHO:
      /* 把收到的首字节编码进返回值，验证参数确实到达服务端 */
      return (void*)(long)(0x1000 + (unsigned char)a[0]);
    case FN_LEN:
      /* 返回 (len<<16) | (末字节<<8) | 首字节：一次校验长度与两端内容 */
      return (void*)(long)(((long)len << 16) |
                           ((long)(unsigned char)a[len > 0 ? len - 1 : 0] << 8) |
                           (long)(unsigned char)a[0]);
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
  void* r;
  int ok;
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
    client_run_ex(server, serve);
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
  CHECK(c->api_size == SERVER_SLOTS, "api_size == 64");

  /* 1. 单次往返 + 参数 */
  r = client_call_ex(c, (void*)FN_ECHO, a1, sizeof(a1), 1000);
  CHECK(r == (void*)(long)(0x1000 + 'A'), "echo roundtrip with arg");

  /* 2. payload 长度：大包首尾都到、零长度不越界 */
  {
    char big[BIG_LEN];
    void* rv;
    memset(big, '.', sizeof(big));
    big[0] = 'H';
    big[BIG_LEN - 1] = 'Y';
    rv = client_call_ex(c, (void*)FN_LEN, big, sizeof(big), 1000);
    CHECK(rv == (void*)(long)((BIG_LEN << 16) | ('Y' << 8) | 'H'),
          "payload len 200 with first/last byte");

    rv = client_call_ex(c, (void*)FN_LEN, NULL, 0, 1000);
    CHECK(rv == (void*)0, "payload len 0");
  }

  /* 3. 连续调用：验证槽位在每次调用后归还 */
  ok = 0;
  for (i = 0; i < SEQ_CALLS; i++) {
    char a[] = "Z";
    void* rr = client_call_ex(c, (void*)FN_ECHO, a, sizeof(a), 1000);
    if (rr == (void*)(long)(0x1000 + 'Z')) ok++;
  }
  CHECK(ok == SEQ_CALLS, "32 sequential calls");

  /* 4. 超时：客户端 300ms，服务端约 1.2s，期望 -1 */
  r = client_call_ex(c, (void*)FN_SLOW, NULL, 0, SLOW_TIMEOUT_MS);
  CHECK(r == (void*)-1, "slow call times out");

  /* 5. 超时后恢复：等慢调用跑完，所有槽位都应可再次使用 */
  sleep_ms(SLOW_MS + 800);
  ok = 0;
  for (i = 0; i < RECOVER_CALLS; i++) {
    char a[] = "B";
    void* rr = client_call_ex(c, (void*)FN_ECHO, a, sizeof(a), 2000);
    if (rr == (void*)(long)(0x1000 + 'B')) ok++;
  }
  CHECK(ok == RECOVER_CALLS, "recover after timeout");

  printf("pass=%d fail=%d\n", g_pass, g_fail);

  /* 性能参考（不计入 pass/fail）：顺序调用的单次往返延迟。
   * 用来对比"futex 唤醒"与"usleep 轮询"两种等待策略。 */
  {
    struct timespec t0, t1;
    const int N = 1000;
    int okb = 0;
    long us;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (i = 0; i < N; i++) {
      char a[] = "C";
      void* rr = client_call_ex(c, (void*)FN_ECHO, a, sizeof(a), 1000);
      if (rr == (void*)(long)(0x1000 + 'C')) okb++;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    us = (long)(t1.tv_sec - t0.tv_sec) * 1000000L +
         (long)(t1.tv_nsec - t0.tv_nsec) / 1000L;
    printf("TINFO: bench %d calls ok=%d total=%ldus avg=%ldus %.0f calls/s\n", N,
           okb, us, us / N, (double)N * 1000000.0 / (double)us);
  }

  clirent_un_regist(c);
  kill(pid, SIGKILL);
  printf("\nSummary:\n");
  return g_fail > 0 ? 1 : 0;
}
