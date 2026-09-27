#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 256
#define MAX_ARGS 30
#define MAX_ARG_LEN 128

int main(int argc, char* argv[]) {
  FILE* file;
  char line[MAX_LINE];
  int child_count = 0;

  file = fopen("/conf/init.conf", "r");

  if (file == NULL) {
    printf("%s: init.conf : %s\n", argv[0], strerror(errno));
    return -1;
  }

  while (fgets(line, sizeof(line), file) != NULL) {
    if (line[0] == '#' || line[0] == '\n') {
      continue;
    }

    // 移除换行符
    int s = strlen(line);
    if (s > 0 && line[s - 1] == '\n') {
      line[s - 1] = 0;
    }

    printf("line: %s\n", line);

    // 在栈上分配参数存储
    char proc_copy[MAX_ARG_LEN];
    char args_copy[MAX_ARGS][MAX_ARG_LEN];
    char* args[MAX_ARGS];

    char* ptr = strtok(line, " ");
    if (ptr == NULL) {
      continue;
    }

    strncpy(proc_copy, ptr, MAX_ARG_LEN - 1);
    proc_copy[MAX_ARG_LEN - 1] = 0;

    int i = 0;
    args[i++] = proc_copy;
    while ((ptr = strtok(NULL, " ")) != NULL && i < MAX_ARGS - 1) {
      strncpy(args_copy[i], ptr, MAX_ARG_LEN - 1);
      args_copy[i][MAX_ARG_LEN - 1] = 0;
      args[i] = args_copy[i];
      i++;
    }
    args[i] = NULL;

    pid_t p1 = fork();
    if (p1 < 0) {
      printf("fork failed\n");
      continue;
    }
    if (p1 == 0) {
      // 子进程
      printf("exec proc %s\n", proc_copy);
      execv(proc_copy, args);
      _exit(1);
    }

    child_count++;

    // 延迟启动下一个进程，避免 XWin 资源竞争
    usleep(500000);  // 500ms
  }

  fclose(file);

  /* YiYiYa: 原来用 waitpid(WNOHANG) + usleep(20000) 每 20ms 轮询，单核下常驻
     约 40% CPU，和模拟器抢核。子进程退出本来就会唤醒父进程，直接阻塞等即可
     （即使子进程是常驻服务不退出，阻塞在这里也是零开销）。 */
  while (child_count > 0) {
    int status;
    pid_t ret = waitpid(-1, &status, 0);
    if (ret > 0) {
      child_count--;
    } else if (ret < 0) {
      if (errno == EINTR) {
        continue;  // 被信号打断，重试
      }
      break;
    }
  }

  return 0;
}