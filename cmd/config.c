#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 256
#define MAX_ARGS 30
#define MAX_ARG_LEN 128

/* 墙钟延时。miyoo 真机上内核的 usleep/nanosleep 基本不生效（实测启动动画
 * 8 步只花了 137ms），所以这里以 xwin 的 get_ticks（syscall 0x5019，毫秒
 * @1kHz）为准；nanosleep 只用来让出 CPU。带迭代上限，绝不挂死。 */
static unsigned int tick_now(void) {
  return (unsigned int)syscall(0x5019);
}

static void delay_ms(unsigned int ms) {
  unsigned int t0 = tick_now();
  unsigned int n = 0;
  if (ms == 0) return;
  while ((unsigned int)(tick_now() - t0) < ms) {
    struct timespec ts;
    if (++n > 3000) break; /* 兜底：tick 不可用时最多再看 3 秒 */
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000L; /* 1ms */
    nanosleep(&ts, NULL);
  }
}

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

    /* YiYiYa: "wait <prog> [args]" 前缀 = 【前台运行】——等它退出后再继续
     * 读下一行。用于 /bin/logo 这种启动动画：必须完整播完，否则会被
     * 500ms 后就拉起的 /bin/ymain 直接盖住。不带前缀的仍是后台 + 500ms。 */
    int wait_proc = 0;
    if (strncmp(line, "wait ", 5) == 0) {
      wait_proc = 1;
      memmove(line, line + 5, strlen(line + 5) + 1);
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

    if (wait_proc) {
      /* "wait <prog>"：给这个程序一段"独占启动"的时间（默认 2.5s）。
       * 【故意不 waitpid】本内核是在 waitpid 回收时才真正释放僵尸线程的
       * vm/页（见 duck/kernel/thread.c 的注释）。提前回收 ⇒ 紧接着 fork 的
       * 全屏 app 会复用刚释放的那批页；miyoo 真机上表现为后者一启动就
       * PREF ABORT（QEMU 不模拟 cache/TLB，看不出）。所以这里只延时、
       * 不回收；真正的回收交给文件读完后的 waitpid 循环。 */
      printf("config: head-start %dms for %s (no reap)\n", 2500, proc_copy);
      delay_ms(2500);
      child_count++;
      continue;
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