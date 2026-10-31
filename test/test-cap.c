/*******************************************************************
 * foot capability（阶段 4）回归测试
 *   1) 按名字申请设备窗口句柄，info 能取到内核登记的基址/长度
 *   2) 窗口内映射成功；越过窗口、未页对齐一律拒绝
 *   3) 派生：只能减权（带 GRANT 才可派生，提权被拒）
 *   4) 伪造句柄失效；撤销父 cap 后派生出的子句柄同时失效
 *   5) 裸 sys_thread_map 映射窗口外物理地址被内核拒绝（收权生效）
 *   6) 撤销后槽位可复用（cap 表没有泄漏）
 *   7) OBJ_FRAME：申请物理页对象、映射读写、越界/未对齐拒绝、
 *      该页让裸 sys_thread_map 放行、派生掉 FRAME_MAP 后不能映射
 *   8) OBJ_ENDPOINT：空队列/无 GRANT 拒绝；fork 后父投递一条带能力的消息，
 *      子进程取到新句柄并能直接拿来映射（跨进程能力转授）
 *
 * 跑法（设备端）：/bin/test-cap
 *   主机侧：python3 tools/ltp_batch.py --timeout 60 /bin/test-cap
 ********************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
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

  /* ===================== OBJ_FRAME（物理页对象） =====================
   * 帧对象让"普通内存页"也能被授权：以前 sys_thread_map 只认设备窗口，
   * 任何一块 RAM 物理页都无法用能力描述，用户态拿着 PA 就能映射。 */
  unsigned int fpa = 0, fsz = 0;
  foot_cap_t fr = foot_cap_alloc_frame(0, 0, 0, &fpa, &fsz);
  CHECK(fr != FOOT_CAP_NONE, "alloc frame object");
  CHECK(fsz == 4096, "frame is one page by default");
  CHECK(fpa != 0 && (fpa & 0xfff) == 0, "frame pa is page aligned");

  unsigned char* fva = (unsigned char*)0x68000000UL;
  CHECK(foot_cap_map(fr, fva, 0, 4096) == 0, "map frame into user va");
  fva[0] = 0x5a;
  fva[4095] = 0xa5;
  CHECK(fva[0] == 0x5a && fva[4095] == 0xa5, "frame page is writable");

  CHECK(foot_cap_map(fr, fva + 0x2000, 4096, 4096) == -1,
        "map beyond frame rejected");
  CHECK(foot_cap_map(fr, fva + 0x100, 0, 4096) == -1,
        "map unaligned va rejected");
  CHECK(foot_cap_map(fr, fva + 0x1000, 0, 0x3000) == -1,
        "map length beyond frame rejected");

  /* 帧能力让裸 sys_thread_map 放行（映射授权闭环） */
  ret = syscall(SYS_THREAD_MAP, (long)gettid(), (long)0x68004000UL, (long)fpa,
                (long)0x1000, (long)3);
  CHECK(ret == 0, "raw SYS_THREAD_MAP accepted for granted frame");

  /* 派生不带 FRAME_MAP ⇒ 能拿句柄但映射被拒（不增原则） */
  foot_cap_t ro2 = foot_cap_derive(fr, CAP_RIGHT_LOAD);
  CHECK(ro2 != FOOT_CAP_NONE, "mint frame cap without map right");
  CHECK(foot_cap_map(ro2, fva + 0x3000, 0, 4096) == -1,
        "minted cap without FRAME_MAP cannot map");

  CHECK(foot_cap_unmap(fr) == 0, "unmap frame");
  foot_cap_revoke(ro2);
  foot_cap_revoke(fr);
  CHECK(foot_cap_info(fr, &pa, &size, &rights) == -1,
        "frame handle dead after revoke");

  /* ===================== OBJ_ENDPOINT（IPC 携带能力） ===================== */
  foot_cap_t ep = foot_cap_ep_create();
  CHECK(ep != FOOT_CAP_NONE, "create endpoint");

  char rbuf[64];
  unsigned long rlen = 0;
  foot_cap_t got = FOOT_CAP_NONE;
  CHECK(foot_cap_ep_try_recv(ep, rbuf, sizeof(rbuf), &rlen, &got) == -1,
        "non-blocking recv on empty endpoint rejected");

  foot_cap_t xfer = foot_cap_get_mmio("dwc2");
  CHECK(xfer != FOOT_CAP_NONE, "acquire cap to transfer");
  foot_cap_t no_grant = foot_cap_derive(xfer, CAP_RIGHT_LOAD);
  CHECK(no_grant != FOOT_CAP_NONE, "mint cap without GRANT");
  CHECK(foot_cap_ep_try_send(ep, "x", 1, no_grant) == -1,
        "cannot transfer cap without GRANT");
  foot_cap_revoke(no_grant);

  /* 父投递 + 附带能力，fork 出的子进程（继承同一 endpoint 句柄）取走并直接使用。
   * 【先 send 再 fork】消息与能力引用都在内核队列里，与谁先跑无关 ——
   * 本内核"父挂起 + 子阻塞"同时成立时调度没有落点（见文档 §5.11 末尾），
   * 测试不依赖这个时序。 */
  CHECK(foot_cap_ep_send(ep, "withcap", 7, xfer) == 0,
        "send message carrying a cap");

  pid_t ep_pid = fork();
  if (ep_pid == 0) {
    int fails = 0;
    char cbuf[64];
    unsigned long clen = 0;
    foot_cap_t cgot = FOOT_CAP_NONE;
    if (foot_cap_ep_recv(ep, cbuf, sizeof(cbuf), &clen, &cgot) != 0) {
      fails++;
    }
    if (cgot == FOOT_CAP_NONE) {
      fails++; /* 没收到消息/能力 */
    } else {
      if (clen != 7 || memcmp(cbuf, "withcap", 7) != 0) {
        fails++; /* 消息体不对 */
      }
      unsigned int gp = 0, gs = 0, gr = 0;
      if (foot_cap_info(cgot, &gp, &gs, &gr) != 0) {
        fails++; /* 收到的句柄在子进程 CSpace 里不可用 */
      }
      if (gs != 0x10000) {
        fails++; /* 应是 dwc2 窗口大小 */
      }
      if ((gr & (CAP_RIGHT_GRANT | CAP_RIGHT_MINT | CAP_RIGHT_REVOKE)) != 0) {
        fails++; /* 转授不能顺带把分发权给出去 */
      }
      if (foot_cap_map(cgot, (void*)(unsigned long)gp, 0, 0x1000) != 0) {
        fails++; /* 收到的能力必须能直接拿来映射 */
      }
    }
    _exit(fails == 0 ? 0 : 1);
  }

  int est = 0;
  waitpid(ep_pid, &est, 0);
  CHECK(WIFEXITED(est) && WEXITSTATUS(est) == 0,
        "child received message and usable cap over endpoint");

  /* 父这边队列已空，能力引用随之转移（父仍然持有自己的 xfer） */
  CHECK(foot_cap_info(xfer, &pa, &size, &rights) == 0,
        "sender cap survives transfer (object refcount)");
  foot_cap_revoke(xfer);
  foot_cap_revoke(ep);

  /* 队列容量：满 8 条时非阻塞 send 失败；腾出一个空位后 send 立刻成功。
   * 【不用"子挂起等父取走"的时序】本内核里"一方真正挂起、另一方随后才动作"
   * 的组合存在调度缺陷（见文档 §5.11 末），测试不去踩它。 */
  foot_cap_t ep2 = foot_cap_ep_create();
  CHECK(ep2 != FOOT_CAP_NONE, "create endpoint for blocking send");
  char one[2] = {'A', 0};
  int filled = 0;
  for (int i = 0; i < 8; i++) {
    one[0] = (char)('A' + i);
    if (foot_cap_ep_try_send(ep2, one, 1, FOOT_CAP_NONE) == 0) {
      filled++;
    }
  }
  CHECK(filled == 8, "fill endpoint queue to capacity");
  CHECK(foot_cap_ep_try_send(ep2, "x", 1, FOOT_CAP_NONE) == -1,
        "non-blocking send on full queue rejected");

  unsigned long blen = 0;
  int got_one = foot_cap_ep_try_recv(ep2, rbuf, sizeof(rbuf), &blen, NULL);
  CHECK(got_one == 0 && blen == 1 && rbuf[0] == 'A',
        "receive one message to free a slot");

  pid_t bpid = fork();
  if (bpid == 0) {
    /* 已有空位 ⇒ 阻塞版 send 立刻成功（子进程里验证跨进程写入） */
    _exit(foot_cap_ep_send(ep2, "wake", 4, FOOT_CAP_NONE) == 0 ? 0 : 1);
  }
  int bst = 0;
  waitpid(bpid, &bst, 0);
  CHECK(WIFEXITED(bst) && WEXITSTATUS(bst) == 0,
        "blocking send succeeds once a slot is free");

  /* 取出剩下 7 条 + 被唤醒那条，最后应是 wake（FIFO 尾插） */
  int tail_ok = 0;
  for (int i = 0; i < 8; i++) {
    char tb[16];
    unsigned long tl = 0;
    if (foot_cap_ep_try_recv(ep2, tb, sizeof(tb), &tl, NULL) != 0) {
      break;
    }
    if (i == 7 && tl == 4 && memcmp(tb, "wake", 4) == 0) {
      tail_ok = 1;
    }
  }
  CHECK(tail_ok, "woken sender's message lands at queue tail");
  foot_cap_revoke(ep2);

  printf("=== test-cap: pass=%d fail=%d ===\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
