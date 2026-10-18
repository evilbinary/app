/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_getresgid_getresgid02
#include "ltp/testcases/kernel/syscalls/getresgid/getresgid02.c"
#undef main
int LTP_OLD_MAIN_getresgid_getresgid02();
int ltp_run_getresgid_getresgid02(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_getresgid_getresgid02)(argc, argv);
}
struct tst_test *ltp_get_getresgid_getresgid02(void);
struct tst_test *ltp_get_getresgid_getresgid02(void) { return (void *)0; }
