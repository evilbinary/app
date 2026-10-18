/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_semctl_semctl06
#include "ltp/testcases/kernel/syscalls/semctl/semctl06.c"
#undef main
int LTP_OLD_MAIN_semctl_semctl06();
int ltp_run_semctl_semctl06(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_semctl_semctl06)(argc, argv);
}
struct tst_test *ltp_get_semctl_semctl06(void);
struct tst_test *ltp_get_semctl_semctl06(void) { return (void *)0; }
