/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_writev_writev05
#include "ltp/testcases/kernel/syscalls/writev/writev05.c"
#undef main
int LTP_OLD_MAIN_writev_writev05();
int ltp_run_writev_writev05(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_writev_writev05)(argc, argv);
}
struct tst_test *ltp_get_writev_writev05(void);
struct tst_test *ltp_get_writev_writev05(void) { return (void *)0; }
