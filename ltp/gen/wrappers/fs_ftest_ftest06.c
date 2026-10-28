/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_fs_ftest_ftest06
#include "ltp/testcases/kernel/fs/ftest/ftest06.c"
#undef main
int LTP_OLD_MAIN_fs_ftest_ftest06();
int ltp_run_fs_ftest_ftest06(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_fs_ftest_ftest06)(argc, argv);
}
struct tst_test *ltp_get_fs_ftest_ftest06(void);
struct tst_test *ltp_get_fs_ftest_ftest06(void) { return (void *)0; }
