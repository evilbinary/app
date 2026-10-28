/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_io_writetest_writetest
#include "ltp/testcases/kernel/io/writetest/writetest.c"
#undef main
int LTP_OLD_MAIN_io_writetest_writetest();
int ltp_run_io_writetest_writetest(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_io_writetest_writetest)(argc, argv);
}
struct tst_test *ltp_get_io_writetest_writetest(void);
struct tst_test *ltp_get_io_writetest_writetest(void) { return (void *)0; }
