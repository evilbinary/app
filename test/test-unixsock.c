/* AF_UNIX 流式套接字最小闭环测试（bind/listen/connect/poll/accept/read/write）。
 *
 * 目的：Xorg 与客户端之间只用到这几个动作，一旦 X 连上却"没反应"，
 * 就是这个链条某一环在内核侧没实现好。这个程序把每一环的返回值打出来，
 * 一眼能看出断在哪：
 *     socket -> bind -> listen -> [对端] connect/write
 *     -> poll(listen) -> accept -> read -> write 回复 -> 对端读到
 *
 * 用法（QEMU raspi3，root 是 FAT 根）：
 *     /bin/test-unixsock            # 默认 poll 等 3000ms
 *     /bin/test-unixsock select     # 用 select 而不是 poll
 *     /bin/test-unixsock 10000      # 自定义超时(ms)
 */
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define SOCK_PATH "/tmp/.ut-unixsock"

static void report(const char* who, const char* what, int ret, int err) {
  printf("[%s] %-12s ret=%-4d errno=%d(%s)\n", who, what, ret, err,
         err ? strerror(err) : "ok");
  fflush(stdout);
}

int main(int argc, char* argv[]) {
  struct sockaddr_un addr;
  int use_select = 0;
  int timeout_ms = 3000;
  int ls, as, n, i;
  pid_t pid;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "select") == 0) {
      use_select = 1;
    } else {
      timeout_ms = atoi(argv[i]);
      if (timeout_ms <= 0) {
        timeout_ms = 3000;
      }
    }
  }

  printf("=== test-unixsock path=%s wait=%s %dms\n", SOCK_PATH,
         use_select ? "select" : "poll", timeout_ms);
  fflush(stdout);

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);
  unlink(SOCK_PATH);

  ls = socket(AF_UNIX, SOCK_STREAM, 0);
  report("srv", "socket", ls, errno);
  if (ls < 0) {
    return 1;
  }

  errno = 0;
  n = bind(ls, (struct sockaddr*)&addr, sizeof(addr));
  report("srv", "bind", n, errno);
  if (n < 0) {
    return 1;
  }

  errno = 0;
  n = listen(ls, 8);
  report("srv", "listen", n, errno);
  if (n < 0) {
    return 1;
  }

  pid = fork();
  if (pid == 0) {
    /* ---- 客户端：X 客户端侧的最小模拟 ---- */
    int cs;
    char buf[32];
    struct sockaddr_un ca;

    sleep(1); /* 让服务端先进入 poll/select，和 Xorg 的时序一致 */
    memset(&ca, 0, sizeof(ca));
    ca.sun_family = AF_UNIX;
    strncpy(ca.sun_path, SOCK_PATH, sizeof(ca.sun_path) - 1);

    errno = 0;
    cs = socket(AF_UNIX, SOCK_STREAM, 0);
    report("cli", "socket", cs, errno);
    if (cs < 0) {
      _exit(1);
    }

    errno = 0;
    n = connect(cs, (struct sockaddr*)&ca, sizeof(ca));
    report("cli", "connect", n, errno);
    if (n < 0) {
      _exit(1);
    }

    errno = 0;
    n = write(cs, "ping", 4);
    report("cli", "write ping", n, errno);

    memset(buf, 0, sizeof(buf));
    errno = 0;
    n = read(cs, buf, sizeof(buf) - 1);
    report("cli", "read reply", n, errno);
    printf("[cli] reply=\"%s\"\n", buf);
    fflush(stdout);
    close(cs);
    _exit(0);
  }

  if (pid < 0) {
    report("srv", "fork", (int)pid, errno);
    return 1;
  }

  /* ---- 服务端：Xorg 侧等待新连接的方式 ---- */
  if (use_select) {
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(ls, &rfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    errno = 0;
    n = select(ls + 1, &rfds, NULL, NULL, &tv);
    report("srv", "select", n, errno);
    printf("[srv] listen readable=%d\n", FD_ISSET(ls, &rfds) ? 1 : 0);
    fflush(stdout);
  } else {
    struct pollfd p;
    p.fd = ls;
    p.events = POLLIN;
    p.revents = 0;
    errno = 0;
    n = poll(&p, 1, timeout_ms);
    report("srv", "poll", n, errno);
    printf("[srv] poll revents=0x%x\n", p.revents);
    fflush(stdout);
  }

  errno = 0;
  as = accept(ls, NULL, NULL);
  report("srv", "accept", as, errno);
  if (as >= 0) {
    char buf[32];
    memset(buf, 0, sizeof(buf));
    errno = 0;
    n = read(as, buf, sizeof(buf) - 1);
    report("srv", "read req", n, errno);
    printf("[srv] req=\"%s\"\n", buf);
    fflush(stdout);

    errno = 0;
    n = write(as, "pong", 4);
    report("srv", "write pong", n, errno);
    close(as);
  }

  close(ls);
  unlink(SOCK_PATH);

  {
    int st = 0;
    waitpid(pid, &st, 0);
    printf("=== test-unixsock done (child status %d)\n", st);
    fflush(stdout);
  }
  return 0;
}
