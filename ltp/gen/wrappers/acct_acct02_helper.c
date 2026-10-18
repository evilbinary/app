/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_acct_acct02_helper
#include "ltp/testcases/kernel/syscalls/acct/acct02_helper.c"
#undef main
int LTP_OLD_MAIN_acct_acct02_helper();
int ltp_run_acct_acct02_helper(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_acct_acct02_helper)(argc, argv);
}
struct tst_test *ltp_get_acct_acct02_helper(void);
struct tst_test *ltp_get_acct_acct02_helper(void) { return (void *)0; }
