/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_sigaction_sigaction01
#include "ltp/testcases/kernel/syscalls/sigaction/sigaction01.c"
#undef main
int LTP_OLD_MAIN_sigaction_sigaction01();
int ltp_run_sigaction_sigaction01(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_sigaction_sigaction01)(argc, argv);
}
struct tst_test *ltp_get_sigaction_sigaction01(void);
struct tst_test *ltp_get_sigaction_sigaction01(void) { return (void *)0; }
