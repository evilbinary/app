/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_sendto_sendto01
#include "ltp/testcases/kernel/syscalls/sendto/sendto01.c"
#undef main
int LTP_OLD_MAIN_sendto_sendto01();
int ltp_run_sendto_sendto01(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_sendto_sendto01)(argc, argv);
}
struct tst_test *ltp_get_sendto_sendto01(void);
struct tst_test *ltp_get_sendto_sendto01(void) { return (void *)0; }
