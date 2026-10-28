/* memfd_create + mmap 共享 + SCM_RIGHTS 传递 —— wl_shm 全链路验收。
 *
 * 流程：memfd_create → ftruncate 定长 → mmap(MAP_SHARED) 写标记 →
 * fork 后父进程把 fd 用 sendmsg(SCM_RIGHTS) 发给子进程 → 子进程
 * mmap 同一 fd 读标记（memfd 建在 /tmp 的文件后端，双方映射同一份页）。
 * 输出 LTP 风格 TPASS/TFAIL + Summary，tools/ltp_batch.py 直跑。 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define TAG "MEMFD-OK"
#define TAGLEN 8
#define POOL_SIZE 8192

static int do_child(int sock) {
  char ctl[CMSG_SPACE(sizeof(int))];
  char buf[32];
  struct iovec iov;
  struct msghdr msg;
  struct cmsghdr* c;
  int passed = -1;
  ssize_t n;
  unsigned char* p;
  int fail = 0;

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
  c = CMSG_FIRSTHDR(&msg);
  if (c == NULL || c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS) {
    printf("TFAIL: child no SCM_RIGHTS cmsg\n");
    return 1;
  }
  memcpy(&passed, CMSG_DATA(c), sizeof(int));
  printf("TPASS: child got memfd fd=%d\n", passed);

  p = mmap(NULL, POOL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, passed, 0);
  if (p == MAP_FAILED) {
    printf("TFAIL: child mmap errno=%d\n", errno);
    return 1;
  }
  if (memcmp(p, TAG, TAGLEN) == 0) {
    printf("TPASS: child mmap read [%s]\n", p);
  } else {
    fail = 1;
    printf("TFAIL: child mmap read mismatch: %.8s\n", p);
  }
  munmap(p, POOL_SIZE);
  return fail;
}

int main(int argc, char* argv[]) {
  int sv[2];
  int fd;
  pid_t pid;
  int st = 0;
  unsigned char* p;
  char ctl[CMSG_SPACE(sizeof(int))];
  struct iovec iov;
  struct msghdr msg;
  struct cmsghdr* c;
  ssize_t n;

  fd = memfd_create("wlshm-test", 0);
  if (fd < 0) {
    printf("TFAIL: memfd_create errno=%d\n", errno);
    return 1;
  }
  printf("TPASS: memfd_create fd=%d\n", fd);

  if (ftruncate(fd, POOL_SIZE) != 0) {
    printf("TFAIL: ftruncate errno=%d\n", errno);
    return 1;
  }
  printf("TPASS: ftruncate %d\n", POOL_SIZE);

  p = mmap(NULL, POOL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (p == MAP_FAILED) {
    printf("TFAIL: parent mmap errno=%d\n", errno);
    return 1;
  }
  memcpy(p, TAG, TAGLEN);
  printf("TPASS: parent mmap write [%s]\n", p);

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
    printf("TFAIL: socketpair errno=%d\n", errno);
    return 1;
  }
  pid = fork();
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
  iov.iov_base = (void*)"go";
  iov.iov_len = 2;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = ctl;
  msg.msg_controllen = sizeof(ctl);
  n = sendmsg(sv[0], &msg, 0);
  if (n == 2) {
    printf("TPASS: sendmemfd -> %zd\n", n);
  } else {
    printf("TFAIL: sendmemfd -> %zd errno=%d\n", n, errno);
  }
  close(fd);

  waitpid(pid, &st, 0);
  printf("TINFO: child exit=%d\n", WIFEXITED(st) ? WEXITSTATUS(st) : -1);
  printf("\nSummary:\n");
  return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}
