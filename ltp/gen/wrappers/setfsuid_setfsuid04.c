/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_setfsuid_setfsuid04
#include "ltp/testcases/kernel/syscalls/setfsuid/setfsuid04.c"
#undef main
int LTP_OLD_MAIN_setfsuid_setfsuid04();
int ltp_run_setfsuid_setfsuid04(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_setfsuid_setfsuid04)(argc, argv);
}
struct tst_test *ltp_get_setfsuid_setfsuid04(void);
struct tst_test *ltp_get_setfsuid_setfsuid04(void) { return (void *)0; }
