/* kill：向进程发送信号。用法：
 *   kill <pid> [signal]     默认 SIGTERM(15)
 * 原来的实现只调用 kill(pid, 0)（探测进程是否存在），没法结束进程。
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  int pid;
  int sig = SIGTERM;

  if (argc <= 1) {
    fprintf(stderr, "usage: kill <pid> [signal]\n");
    return 1;
  }
  pid = atoi(argv[1]);
  if (argc > 2) {
    sig = atoi(argv[2]);
  }
  if (kill(pid, sig) != 0) {
    fprintf(stderr, "kill %d sig %d failed: %s\n", pid, sig, strerror(errno));
    return 1;
  }
  return 0;
}
