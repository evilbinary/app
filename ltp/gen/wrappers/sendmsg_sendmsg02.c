/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_sendmsg_sendmsg02
#include "ltp/testcases/kernel/syscalls/sendmsg/sendmsg02.c"
#undef main
int LTP_OLD_MAIN_sendmsg_sendmsg02();
int ltp_run_sendmsg_sendmsg02(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_sendmsg_sendmsg02)(argc, argv);
}
struct tst_test *ltp_get_sendmsg_sendmsg02(void);
struct tst_test *ltp_get_sendmsg_sendmsg02(void) { return (void *)0; }
