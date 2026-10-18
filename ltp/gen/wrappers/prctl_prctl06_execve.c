/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_prctl_prctl06_execve
#include "ltp/testcases/kernel/syscalls/prctl/prctl06_execve.c"
#undef main
int LTP_OLD_MAIN_prctl_prctl06_execve();
int ltp_run_prctl_prctl06_execve(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_prctl_prctl06_execve)(argc, argv);
}
struct tst_test *ltp_get_prctl_prctl06_execve(void);
struct tst_test *ltp_get_prctl_prctl06_execve(void) { return (void *)0; }
