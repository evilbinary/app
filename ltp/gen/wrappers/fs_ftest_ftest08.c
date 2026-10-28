/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_fs_ftest_ftest08
#include "ltp/testcases/kernel/fs/ftest/ftest08.c"
#undef main
int LTP_OLD_MAIN_fs_ftest_ftest08();
int ltp_run_fs_ftest_ftest08(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_fs_ftest_ftest08)(argc, argv);
}
struct tst_test *ltp_get_fs_ftest_ftest08(void);
struct tst_test *ltp_get_fs_ftest_ftest08(void) { return (void *)0; }
