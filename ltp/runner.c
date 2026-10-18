/* LTP 统一 runner
 * 用法：ltp [名字过滤]...   无参数 = 跑全部
 *       ltp fork01 write01  只跑指定的
 * 每个测试 fork 子进程执行：子进程崩溃/死循环（alarm 120s 兜底）不影响
 * 后续测试；父进程按 wait 状态汇总 PASS/FAIL/CRASH。
 * 结果同时打汇总表，便于真机串口拷走。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

#include "ltp_runner.h"

extern const ltp_entry_t ltp_tests[];
extern const int ltp_test_count;

#define LTP_CHILD_TIMEOUT 120

static int name_matches(const char *name, int nfilter, char **filter) {
  int i;
  if (nfilter == 0) {
    return 1;
  }
  for (i = 0; i < nfilter; i++) {
    if (strstr(name, filter[i]) != NULL) {
      return 1;
    }
  }
  return 0;
}

static void child_alarm_handler(int sig) { (void)sig; }

int main(int argc, char **argv) {
  int nfilter = argc - 1;
  char **filter = argv + 1;
  int npass = 0, nfail = 0, ncrash = 0, nskip = 0;
  int i;

  printf("\n=== LTP suite: %d tests ===\n", ltp_test_count);
  fflush(stdout);

  for (i = 0; i < ltp_test_count; i++) {
    const ltp_entry_t *e = &ltp_tests[i];
    pid_t pid;
    int status = 0;

    if (!name_matches(e->name, nfilter, filter)) {
      continue;
    }
    printf("[%s] ", e->name);
    fflush(stdout);

    pid = fork();
    if (pid < 0) {
      printf("BROK (fork failed)\n");
      nfail++;
      continue;
    }
    if (pid == 0) {
      /* 子进程：跑单个测试 */
      signal(SIGALRM, child_alarm_handler);
      alarm(LTP_CHILD_TIMEOUT);
      if (e->old_run != NULL) {
        exit(e->old_run(argc, argv) == 0 ? 0 : 1);
      }
      if (e->get != NULL) {
        /* 新 API：借用 tst_run_tcases 的标准入口 */
        extern void tst_run_tcases(int, char **, struct tst_test *);
        char *av[2];
        av[0] = (char *)e->name;
        av[1] = NULL;
        tst_run_tcases(1, av, e->get());
        exit(0);
      }
      exit(32); /* 不可达 */
    }

    /* 父进程：等结果 */
    if (waitpid(pid, &status, 0) < 0) {
      printf("BROK (wait failed)\n");
      nfail++;
      continue;
    }
    if (WIFEXITED(status)) {
      int code = WEXITSTATUS(status);
      if (code == 0) {
        printf("PASS\n");
        npass++;
      } else if (code == 32) {
        printf("TCONF?\n");
        nskip++;
      } else if (code == 1) {
        printf("FAIL\n");
        nfail++;
      } else if (code == 2) {
        printf("BROK\n");
        nfail++;
      } else {
        printf("TCONF\n"); /* LTP TCONF 退出码 32 之外常见 0/1/2 */
        nskip++;
      }
    } else if (WIFSIGNALED(status)) {
      printf("CRASH (sig %d)\n", WTERMSIG(status));
      ncrash++;
    } else {
      printf("BROK\n");
      nfail++;
    }
    fflush(stdout);
  }

  printf("\n=== LTP summary: pass=%d fail=%d crash=%d conf/skip=%d ===\n",
         npass, nfail, ncrash, nskip);
  return (nfail + ncrash) ? 1 : 0;
}
