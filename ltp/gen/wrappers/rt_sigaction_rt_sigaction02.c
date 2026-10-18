/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_rt_sigaction_rt_sigaction02
#include "ltp/testcases/kernel/syscalls/rt_sigaction/rt_sigaction02.c"
#undef main
int LTP_OLD_MAIN_rt_sigaction_rt_sigaction02();
int ltp_run_rt_sigaction_rt_sigaction02(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_rt_sigaction_rt_sigaction02)(argc, argv);
}
struct tst_test *ltp_get_rt_sigaction_rt_sigaction02(void);
struct tst_test *ltp_get_rt_sigaction_rt_sigaction02(void) { return (void *)0; }
