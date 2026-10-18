/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mincore_mincore01
#include "ltp/testcases/kernel/syscalls/mincore/mincore01.c"
#undef main
int LTP_OLD_MAIN_mincore_mincore01();
int ltp_run_mincore_mincore01(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mincore_mincore01)(argc, argv);
}
struct tst_test *ltp_get_mincore_mincore01(void);
struct tst_test *ltp_get_mincore_mincore01(void) { return (void *)0; }
