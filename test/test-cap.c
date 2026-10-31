/*******************************************************************
 * foot capability（阶段 4）回归测试
 *   1) 按名字申请设备窗口句柄，info 能取到内核登记的基址/长度
 *   2) 窗口内映射成功；越过窗口、未页对齐一律拒绝
 *   3) 派生：只能减权（带 GRANT 才可派生，提权被拒）
 *   4) 伪造句柄失效；撤销父 cap 后派生出的子句柄同时失效
 *   5) 裸 sys_thread_map 映射窗口外物理地址被内核拒绝（收权生效）
 *   6) 撤销后槽位可复用（cap 表没有泄漏）
 *
 * 跑法（设备端）：/bin/test-cap
 *   主机侧：python3 tools/ltp_batch.py --timeout 60 /bin/test-cap
 ********************************************************************/
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include "client.h"

/* 自定义 syscall 号（与 duck/modules/posix/sysfn_no.h 一致） */
#define SYS_THREAD_MAP 518

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

int main(int argc, char* argv[]) {
  unsigned int pa = 0, size = 0, rights = 0;
  long ret;

  /* 1) 窗口申请与查询 */
  foot_cap_t cap = foot_cap_get_mmio("gpio");
  CHECK(cap != FOOT_CAP_NONE, "get mmio window 'gpio'");
  if (cap == FOOT_CAP_NONE) {
    printf("=== test-cap: pass=%d fail=%d ===\n", g_pass, g_fail);
    return 1;
  }

  CHECK(foot_cap_info(cap, &pa, &size, &rights) == 0, "cap info");
  CHECK(size == 0x1000, "window size is 0x1000");
  CHECK(pa != 0, "window base is registered by kernel");
  CHECK((rights & CAP_DEVWIN_MAP) != 0, "window granted CAP_DEVWIN_MAP");
  printf("info: pa=0x%x size=0x%x rights=0x%x\n", pa, size, rights);

  /* 2) 映射：窗口内成功；越界/未对齐被拒 */
  CHECK(foot_cap_map(cap, (void*)(unsigned long)pa, 0, 0x1000) == 0,
        "map window inside range");
  CHECK(foot_cap_map(cap, (void*)(unsigned long)pa, 0, 0x2000) == -1,
        "map beyond window rejected");
  CHECK(foot_cap_map(cap, (void*)(unsigned long)pa, 0, 0x100) == -1,
        "map unaligned length rejected");

  /* 3) mint（派生）：只能减权。v2 语义是 new = parent & desired，
   *    所以"要更多"不会失败，而是被裁剪 —— 正是不增原则的体现。 */
  foot_cap_t ro = foot_cap_derive(cap, CAP_RIGHT_LOAD | CAP_DEVWIN_MAP);
  CHECK(ro != FOOT_CAP_NONE, "mint read-only child");
  unsigned int pa4 = 0, sz4 = 0, rt4 = 0;
  CHECK(foot_cap_info(ro, &pa4, &sz4, &rt4) == 0 && rt4 == (CAP_RIGHT_LOAD | CAP_DEVWIN_MAP),
        "minted child rights are exactly the intersection");
  foot_cap_t esc = foot_cap_derive(cap, CAP_RIGHT_LOAD | CAP_RIGHT_DESTROY);
  CHECK(esc != FOOT_CAP_NONE, "mint with extra right still succeeds (clipped)");
  unsigned int pa5 = 0, sz5 = 0, rt5 = 0;
  CHECK(foot_cap_info(esc, &pa5, &sz5, &rt5) == 0 &&
            (rt5 & CAP_RIGHT_DESTROY) == 0,
        "mint cannot escalate (DESTROY stripped)");

  /* 4) 未授权窗口 / 伪造句柄 */
  CHECK(foot_cap_get_mmio("no-such-window") == FOOT_CAP_NONE,
        "unknown window name rejected");
  CHECK(foot_cap_info(0xDEADBEEFu, &pa, &size, &rights) == -1,
        "forged handle rejected");

  /* 5) 裸 sys_thread_map 收权：窗口外物理地址必须被拒 */
  ret = syscall(SYS_THREAD_MAP, (long)gettid(), (long)0x10000000UL,
                (long)0x20000000UL, (long)0x1000, (long)1);
  CHECK(ret != 0, "raw SYS_THREAD_MAP outside cap window rejected");

  /* 6) 撤销父 cap：自己与派生句柄同时失效；槽位可复用 */
  CHECK(foot_cap_revoke(cap) == 0, "revoke parent cap");
  CHECK(foot_cap_info(cap, &pa, &size, &rights) == -1,
        "parent handle dead after revoke");
  CHECK(foot_cap_info(ro, &pa, &size, &rights) == -1,
        "derived handle dead after parent revoke");

  foot_cap_t cap2 = foot_cap_get_mmio("dwc2");
  CHECK(cap2 != FOOT_CAP_NONE, "cap slot reusable after revoke");
  foot_cap_revoke(cap2);

  /* ===================== 用户权限模型 ===================== */
  uid_t ru = 1, eu = 1, su = 1;
  CHECK(getresuid(&ru, &eu, &su) == 0 && ru == 0 && eu == 0 && su == 0,
        "getresuid reports real creds (root)");
  CHECK(getuid() == 0 && geteuid() == 0, "getuid/geteuid report root");

  /* fork 的 CSpace 语义（v2 挂 TCB）：
   *   子进程继承父的能力，但去掉 GRANT/MINT/REVOKE 位；
   *   能力路径不看 uid —— 子进程 setuid 降权后，已持有的能力照样可用。 */
  foot_cap_t hi = foot_cap_get_mmio("dwc2");
  CHECK(hi != FOOT_CAP_NONE, "acquire inheritable cap before fork");

  pid_t pid = fork();
  if (pid == 0) {
    int fails = 0;
    unsigned int p2 = 0, s2 = 0, r2 = 0;
    /* 1) 子进程继承同一批能力（同一对象，句柄值一致） */
    if (foot_cap_info(hi, &p2, &s2, &r2) != 0) {
      fails++;
    }
    /* 2) 但不能转授/再派生/撤销（fork 已剥掉这三位） */
    if ((r2 & (CAP_RIGHT_GRANT | CAP_RIGHT_MINT | CAP_RIGHT_REVOKE)) != 0) {
      fails++;
    }
    /* 3) 降权不影响能力：授权只看有没有能力，不看 uid */
    if (setuid(1000) != 0) {
      fails++;
    }
    if (foot_cap_info(hi, &p2, &s2, &r2) != 0) {
      fails++;
    }
    _exit(fails == 0 ? 0 : 1);
  }

  int st = 0;
  waitpid(pid, &st, 0);
  CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0,
        "fork inherits caps stripped of GRANT; uid does not gate caps");
  unsigned int p3 = 0, s3 = 0, r3 = 0;
  CHECK(foot_cap_info(hi, &p3, &s3, &r3) == 0,
        "parent cap survives child exit (object refcount)");
  CHECK(geteuid() == 0, "parent creds unaffected by child setuid");
  foot_cap_revoke(hi);

  printf("=== test-cap: pass=%d fail=%d ===\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
