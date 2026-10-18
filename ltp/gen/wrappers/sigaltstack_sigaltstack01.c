/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_sigaltstack_sigaltstack01
#include "ltp/testcases/kernel/syscalls/sigaltstack/sigaltstack01.c"
#undef main
int LTP_OLD_MAIN_sigaltstack_sigaltstack01();
int ltp_run_sigaltstack_sigaltstack01(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_sigaltstack_sigaltstack01)(argc, argv);
}
struct tst_test *ltp_get_sigaltstack_sigaltstack01(void);
struct tst_test *ltp_get_sigaltstack_sigaltstack01(void) { return (void *)0; }
