/*
 * system() 启动外部程序测试：验证 musl 的 system() -> fork/exec/sh 这条
 * 路径在 YiYiYa 上是否可用（曾出现 syscall 359 = pipe2 未实现导致的告警）。
 *
 * 用法: test-system [cmd]
 *   默认: /bin/gnuboy pokemon.gbc
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
  const char* cmd = (argc > 1) ? argv[1] : "/bin/gnuboy pokemon.gbc";

  printf("test-system: cmd = %s\n", cmd);
  fflush(stdout);

  errno = 0;
  int rc = system(cmd);
  printf("test-system: system() = %d, errno=%d (%s)\n", rc, errno,
         strerror(errno));
  fflush(stdout);

  return 0;
}
