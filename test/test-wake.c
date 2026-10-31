/*******************************************************************
 * 阻塞式等待的"唤醒返回值"回归测试（fork / unix socket / pty / poll）。
 *
 * 复现目标（调度器线程恢复时序）：
 *   唤醒方（thread_wake_ex / wait_fill）把返回值写进 thread->ctx->ksp，
 *   这只在目标线程【已经真正切出】时正确。如果唤醒发生在
 *   "登记为 THREAD_WAITING" → "context_switch 切走" 之间（另一颗核上的
 *   写者、或定时器 tick 在该窗口里到期），目标线程的恢复点用的是共享
 *   运行栈上的 trap 帧（ctx->ic），写进 ksp 的值会被 context_switch 的
 *   kmemcpy(++ksp, ic) 覆盖 —— 于是阻塞 syscall 返回的是【第一个参数】
 *   而不是期望值：
 *       poll()   → 返回 struct pollfd* 指针（远大于 nfds）
 *       read()   → 返回 fd 号码
 *       accept() → 返回 listen fd
 *
 * 判定方式（都在用户态可观测，不需要内核探针）：
 *   poll 的返回值只能是 [-1, nfds]，超时必须恰好 0；
 *   read/accept 的返回值必须符合语义（字节数 / 新 fd != listen fd）。
 *
 * 输出 LTP 风格 TPASS/TFAIL + Summary 行，可直接用
 *   python3 tools/ltp_batch.py --timeout 120 /bin/test-wake
 ********************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

/* 内核 pty 私有 ioctl（同 duck/modules/pty/pty.h） */
#define IOC_PTY_MAGIC 'p'
#define IOC_SLAVE _IOW(IOC_PTY_MAGIC, 1, int)
#define IOC_MASTER_READ_NOBLOCK _IOW(IOC_PTY_MAGIC, 5, int)

static int t_pass = 0;
static int t_fail = 0;

static void ok(const char* name) {
  t_pass++;
  printf("TPASS: %s\n", name);
  fflush(stdout);
}

static void bad(const char* name, long got, long want, int err) {
  t_fail++;
  printf("TFAIL: %s got=%ld want=%ld errno=%d(%s)\n", name, got, want, err,
         err ? strerror(err) : "ok");
  fflush(stdout);
}

/* ------------------------------------------------------------------
 * 1) poll 短超时：没有任何 fd 就绪，poll(1ms) 必须返回 0。
 *    tick 到期唤醒就发生在"登记 → 切出"窗口附近，命中时返回值会变成
 *    fds 指针（巨大的正数）。多轮跑提高命中率。
 * ------------------------------------------------------------------ */
static void test_poll_timeout(int rounds) {
  int pfd[2];
  int i;
  int badcnt = 0;
  long first_bad = 0;

  if (pipe(pfd) != 0) {
    bad("poll_timeout:pipe", -1, 0, errno);
    return;
  }
  for (i = 0; i < rounds; i++) {
    struct pollfd p;
    int r;
    p.fd = pfd[0];
    p.events = POLLIN;
    p.revents = 0;
    errno = 0;
    r = poll(&p, 1, 1);
    if (r != 0) {
      if (badcnt == 0) {
        first_bad = r;
        printf("TINFO: poll_timeout first miss #%d ret=%d revents=0x%x\n", i, r,
               p.revents);
        fflush(stdout);
      }
      badcnt++;
    }
  }
  close(pfd[0]);
  close(pfd[1]);
  if (badcnt == 0) {
    ok("poll_timeout: every poll(1ms) returned 0");
  } else {
    char name[96];
    snprintf(name, sizeof(name), "poll_timeout: %d/%d rounds wrong (first=%ld)",
             badcnt, rounds, first_bad);
    bad(name, first_bad, 0, 0);
  }
}

/* ------------------------------------------------------------------
 * 2) fork + socketpair：父进程 poll 等可读，子进程立刻写。
 *    命中时 poll 返回 fds 指针、read 返回 fd 号码。
 * ------------------------------------------------------------------ */
static void test_fork_socket(int rounds) {
  int i;
  int badcnt = 0;
  long first_bad = 0;

  for (i = 0; i < rounds; i++) {
    int sv[2];
    pid_t pid;
    struct pollfd p;
    char buf[16];
    int r, n, st = 0;
    int wrong = 0;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
      bad("fork_socket:socketpair", -1, 0, errno);
      return;
    }
    pid = fork();
    if (pid == 0) {
      close(sv[0]);
      write(sv[1], "ping", 4);
      close(sv[1]);
      _exit(0);
    }
    if (pid < 0) {
      close(sv[0]);
      close(sv[1]);
      bad("fork_socket:fork", (long)pid, 0, errno);
      return;
    }
    close(sv[1]);

    p.fd = sv[0];
    p.events = POLLIN;
    p.revents = 0;
    errno = 0;
    r = poll(&p, 1, 3000);
    if (r != 1) {
      wrong = 1;
      if (badcnt == 0) {
        printf("TINFO: fork_socket #%d poll ret=%d (fd=%d) revents=0x%x\n", i, r,
               sv[0], p.revents);
        fflush(stdout);
      }
    }
    memset(buf, 0, sizeof(buf));
    errno = 0;
    n = read(sv[0], buf, 4);
    if (n != 4 || memcmp(buf, "ping", 4) != 0) {
      wrong = 1;
      if (badcnt == 0) {
        printf("TINFO: fork_socket #%d read ret=%d data=[%s] (fd=%d)\n", i, n,
               buf, sv[0]);
        fflush(stdout);
      }
    }
    close(sv[0]);
    waitpid(pid, &st, 0);
    if (wrong) {
      if (badcnt == 0) {
        first_bad = n;
      }
      badcnt++;
    }
  }
  if (badcnt == 0) {
    ok("fork_socket: all rounds poll=1 read=4 ping");
  } else {
    char name[96];
    snprintf(name, sizeof(name), "fork_socket: %d/%d rounds wrong", badcnt,
             rounds);
    bad(name, first_bad, 4, 0);
  }
}

/* ------------------------------------------------------------------
 * 3) fork + socketpair 双向 ping-pong：子进程收到 ping 回 pong。
 *    父进程 read 等回复（阻塞读路径，不是 poll）。
 * ------------------------------------------------------------------ */
static void test_fork_pingpong(int rounds) {
  int i;
  int badcnt = 0;

  for (i = 0; i < rounds; i++) {
    int sv[2];
    pid_t pid;
    char buf[16];
    int n, st = 0;
    int wrong = 0;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
      bad("fork_pingpong:socketpair", -1, 0, errno);
      return;
    }
    pid = fork();
    if (pid == 0) {
      char c[8];
      int k;
      close(sv[0]);
      k = read(sv[1], c, 4);
      if (k == 4) {
        write(sv[1], "pong", 4);
      }
      close(sv[1]);
      _exit(0);
    }
    if (pid < 0) {
      close(sv[0]);
      close(sv[1]);
      bad("fork_pingpong:fork", (long)pid, 0, errno);
      return;
    }
    close(sv[1]);
    write(sv[0], "ping", 4);

    memset(buf, 0, sizeof(buf));
    errno = 0;
    n = read(sv[0], buf, 4);
    if (n != 4 || memcmp(buf, "pong", 4) != 0) {
      wrong = 1;
      if (badcnt == 0) {
        printf("TINFO: fork_pingpong #%d read ret=%d data=[%s] (fd=%d)\n", i, n,
               buf, sv[0]);
        fflush(stdout);
      }
    }
    close(sv[0]);
    waitpid(pid, &st, 0);
    if (wrong) {
      badcnt++;
    }
  }
  if (badcnt == 0) {
    ok("fork_pingpong: all rounds read=4 pong");
  } else {
    char name[96];
    snprintf(name, sizeof(name), "fork_pingpong: %d/%d rounds wrong", badcnt,
             rounds);
    bad(name, badcnt, 0, 0);
  }
}

/* ------------------------------------------------------------------
 * 4) pty：open /dev/ptm → fork 子进程在从端 echo → 父进程 poll 主端再读。
 *    pty 从端阻塞读走 sys_rw_wait，主端非阻塞靠 poll —— 两条路径都在
 *    等待层上，命中时 read 返回主端 fd、poll 返回 fds 指针。
 * ------------------------------------------------------------------ */
static void test_pty(int rounds) {
  int i;
  int badcnt = 0;

  for (i = 0; i < rounds; i++) {
    int m, idx;
    char spath[32];
    pid_t pid;
    struct pollfd p;
    char buf[32];
    int r, n, st = 0;
    int wrong = 0;

    m = open("/dev/ptm", O_RDWR);
    if (m < 0) {
      printf("TCONF: pty not available (errno=%d)\n", errno);
      fflush(stdout);
      return;
    }
    ioctl(m, IOC_MASTER_READ_NOBLOCK);
    idx = ioctl(m, IOC_SLAVE);
    snprintf(spath, sizeof(spath), "/dev/pts/%d", idx);

    pid = fork();
    if (pid == 0) {
      int s = open(spath, O_RDWR);
      if (s >= 0) {
        char c[32];
        int k = read(s, c, 5); /* 阻塞读：等父进程写 */
        if (k > 0) {
          write(s, c, (size_t)k); /* echo */
        }
        close(s);
      }
      _exit(0);
    }
    if (pid < 0) {
      close(m);
      bad("pty:fork", (long)pid, 0, errno);
      return;
    }

    write(m, "hello", 5);

    p.fd = m;
    p.events = POLLIN;
    p.revents = 0;
    errno = 0;
    r = poll(&p, 1, 3000);
    {
      /* FIONREAD：poll 说可读时主端 in 缓冲实际有多少字节。
       * poll>0 但 avail==0 ⇒ 就绪判定与 read 条件不一致（虚假就绪）。 */
      int avail = -1;
      ioctl(m, 0x541Bu, &avail);
      if (r != 1 || avail <= 0) {
        printf("TINFO: pty #%d poll ret=%d avail=%d (fd=%d) revents=0x%x\n", i,
               r, avail, m, p.revents);
        fflush(stdout);
      }
    }
    if (r != 1) {
      wrong = 1;
    }
    memset(buf, 0, sizeof(buf));
    errno = 0;
    n = read(m, buf, sizeof(buf) - 1);
    {
      int e = errno;
      int avail2 = -1;
      ioctl(m, 0x541Bu, &avail2);
      if (n != 5 || memcmp(buf, "hello", 5) != 0) {
        wrong = 1;
        if (badcnt == 0) {
          printf("TINFO: pty #%d read ret=%d errno=%d data=[%s] avail_after=%d "
                 "(fd=%d)\n",
                 i, n, e, buf, avail2, m);
          fflush(stdout);
        }
      }
    }
    close(m);
    waitpid(pid, &st, 0);
    if (wrong) {
      badcnt++;
    }
  }
  if (badcnt == 0) {
    ok("pty: all rounds poll=1 read=5 hello");
  } else {
    char name[96];
    snprintf(name, sizeof(name), "pty: %d/%d rounds wrong", badcnt, rounds);
    bad(name, badcnt, 0, 0);
  }
}

int main(int argc, char* argv[]) {
  int rounds_poll = 3000;
  int rounds_io = 60;

  if (argc > 1) {
    int v = atoi(argv[1]);
    if (v > 0) {
      rounds_poll = v;
    }
  }
  if (argc > 2) {
    int v = atoi(argv[2]);
    if (v > 0) {
      rounds_io = v;
    }
  }

  printf("=== test-wake poll_rounds=%d io_rounds=%d\n", rounds_poll, rounds_io);
  fflush(stdout);

  test_poll_timeout(rounds_poll);
  test_fork_socket(rounds_io);
  test_fork_pingpong(rounds_io);
  test_pty(rounds_io);

  printf("=== test-wake Summary: pass=%d fail=%d\n", t_pass, t_fail);
  fflush(stdout);
  return t_fail == 0 ? 0 : 1;
}
