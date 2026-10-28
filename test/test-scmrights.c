/* SCM_RIGHTS fd 传递端到端验证（wayland 地基，阶段 0 验收用例）。
 *
 * 流程：socketpair 建本机通道 → 父进程把一个已写入标记数据的文件 fd
 * 用 sendmsg(SCM_RIGHTS) 发给子进程 → 子进程 recvmsg 收 fd → 直接
 * read 该 fd 验证数据可读。父进程在 sendmsg 后立刻 close 自己的 fd，
 * 验证内核持引用语义（Linux：在途 fd 不因发送方 close 而失效）。
 *
 * 输出 LTP 风格 TPASS/TFAIL + Summary 行：可直接用
 * tools/ltp_batch.py /bin/test-scmrights 跑（runner 按 "Summary:" 判完成、
 * 按 TPASS/TFAIL 计数）。 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define TAG "SCM-OK"
#define TAGLEN 6

static int do_child(int sock) {
  char buf[32];
  char ctl[CMSG_SPACE(sizeof(int)) * 2];
  struct iovec iov;
  struct msghdr msg;
  struct cmsghdr* c;
  int passed = -1;
  ssize_t n;
  char rbuf[TAGLEN + 1];
  int fail = 0;

  memset(buf, 0, sizeof(buf));
  memset(ctl, 0, sizeof(ctl));
  iov.iov_base = buf;
  iov.iov_len = sizeof(buf) - 1;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = ctl;
  msg.msg_controllen = sizeof(ctl);

  n = recvmsg(sock, &msg, 0);
  if (n < 0) {
    printf("TFAIL: child recvmsg errno=%d\n", errno);
    return 1;
  }
  if (n > 0 && n < (ssize_t)sizeof(buf)) {
    buf[n] = 0;
  }
  if (n == 4 && memcmp(buf, "ping", 4) == 0) {
    printf("TPASS: child recv data [%s]\n", buf);
  } else {
    fail = 1;
    printf("TFAIL: child recv %zd bytes [%s]\n", n, buf);
  }

  c = CMSG_FIRSTHDR(&msg);
  if (c == NULL) {
    printf("TFAIL: no cmsg (controllen=%zu)\n", (size_t)msg.msg_controllen);
    return 1;
  }
  if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) {
    printf("TPASS: cmsg level=%d type=%d len=%zu\n", c->cmsg_level,
           c->cmsg_type, (size_t)c->cmsg_len);
  } else {
    fail = 1;
    printf("TFAIL: cmsg level=%d type=%d\n", c->cmsg_level, c->cmsg_type);
  }
  memcpy(&passed, CMSG_DATA(c), sizeof(int));
  printf("TINFO: passed fd=%d\n", passed);

  memset(rbuf, 0, sizeof(rbuf));
  /* 【共享 offset】SCM_RIGHTS 传的是同一 fd_t：父进程 write 已把 offset
   * 推到文件尾，子进程 read 前先归零（Linux 语义本就如此） */
  lseek(passed, 0, SEEK_SET);
  n = read(passed, rbuf, TAGLEN);
  if (n == TAGLEN && memcmp(rbuf, TAG, TAGLEN) == 0) {
    printf("TPASS: read passed fd -> %zd [%s]\n", n, rbuf);
  } else {
    fail = 1;
    printf("TFAIL: read passed fd -> %zd errno=%d [%s]\n", n, errno, rbuf);
  }
  return fail;
}

int main(int argc, char* argv[]) {
  const char* path = (argc > 1) ? argv[1] : "/tmp/scmtest.dat";
  int sv[2];
  int fd;
  pid_t pid;
  int st = 0;
  char ctl[CMSG_SPACE(sizeof(int))];
  struct iovec iov;
  struct msghdr msg;
  struct cmsghdr* c;
  ssize_t n;

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
    printf("parent: socketpair failed errno=%d\n", errno);
    return 1;
  }
  fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    printf("parent: open %s failed errno=%d\n", path, errno);
    return 1;
  }
  if (write(fd, TAG, TAGLEN) != TAGLEN) {
    printf("parent: write failed errno=%d\n", errno);
    return 1;
  }

  pid = fork();
  if (pid < 0) {
    printf("parent: fork failed errno=%d\n", errno);
    return 1;
  }
  if (pid == 0) {
    close(sv[0]);
    return do_child(sv[1]);
  }

  close(sv[1]);
  memset(ctl, 0, sizeof(ctl));
  c = (struct cmsghdr*)ctl;
  c->cmsg_len = CMSG_LEN(sizeof(int));
  c->cmsg_level = SOL_SOCKET;
  c->cmsg_type = SCM_RIGHTS;
  memcpy(CMSG_DATA(c), &fd, sizeof(int));
  iov.iov_base = (void*)"ping";
  iov.iov_len = 4;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = ctl;
  msg.msg_controllen = sizeof(ctl);

  n = sendmsg(sv[0], &msg, 0);
  if (n == 4) {
    printf("TPASS: parent sendmsg data+fd -> %zd\n", n);
  } else {
    printf("TFAIL: parent sendmsg -> %zd errno=%d\n", n, errno);
  }
  close(fd); /* 先于子进程收：验证内核持引用语义 */

  waitpid(pid, &st, 0);
  printf("TINFO: child exit=%d\n", WIFEXITED(st) ? WEXITSTATUS(st) : -1);
  printf("\nSummary:\n");
  return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}
