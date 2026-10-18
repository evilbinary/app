/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_getgroups_getgroups03
#include "ltp/testcases/kernel/syscalls/getgroups/getgroups03.c"
#undef main
int LTP_OLD_MAIN_getgroups_getgroups03();
int ltp_run_getgroups_getgroups03(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_getgroups_getgroups03)(argc, argv);
}
struct tst_test *ltp_get_getgroups_getgroups03(void);
struct tst_test *ltp_get_getgroups_getgroups03(void) { return (void *)0; }
