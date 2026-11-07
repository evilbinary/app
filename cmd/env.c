/*
 * env：最小环境变量启动器（YiYiYa 的 sh 不支持 `VAR=value cmd`，
 * 也没有 /bin/env）。用法：
 *   /bin/env WAYLAND_DEBUG=1 /usr/libexec/weston-desktop-shell
 *   /bin/env XDG_RUNTIME_DIR=/lib /bin/weston-desktop ...
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int i = 1;

  while (i < argc && strchr(argv[i], '=') != NULL) {
    char *eq = strchr(argv[i], '=');
    *eq = '\0';
    if (setenv(argv[i], eq + 1, 1) != 0) {
      perror("env: setenv");
      return 125;
    }
    i++;
  }

  if (i >= argc) {
    fprintf(stderr, "usage: env [VAR=value ...] command [args ...]\n");
    return 125;
  }

  execvp(argv[i], &argv[i]);
  perror("env: execvp");
  return 127;
}
