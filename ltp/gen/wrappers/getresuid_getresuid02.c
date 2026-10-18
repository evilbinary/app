/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_getresuid_getresuid02
#include "ltp/testcases/kernel/syscalls/getresuid/getresuid02.c"
#undef main
int LTP_OLD_MAIN_getresuid_getresuid02();
int ltp_run_getresuid_getresuid02(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_getresuid_getresuid02)(argc, argv);
}
struct tst_test *ltp_get_getresuid_getresuid02(void);
struct tst_test *ltp_get_getresuid_getresuid02(void) { return (void *)0; }
