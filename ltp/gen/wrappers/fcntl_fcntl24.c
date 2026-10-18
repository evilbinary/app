/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_fcntl_fcntl24
#include "ltp/testcases/kernel/syscalls/fcntl/fcntl24.c"
#undef main
int LTP_OLD_MAIN_fcntl_fcntl24();
int ltp_run_fcntl_fcntl24(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_fcntl_fcntl24)(argc, argv);
}
struct tst_test *ltp_get_fcntl_fcntl24(void);
struct tst_test *ltp_get_fcntl_fcntl24(void) { return (void *)0; }
