/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_kill_kill12
#include "ltp/testcases/kernel/syscalls/kill/kill12.c"
#undef main
int LTP_OLD_MAIN_kill_kill12();
int ltp_run_kill_kill12(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_kill_kill12)(argc, argv);
}
struct tst_test *ltp_get_kill_kill12(void);
struct tst_test *ltp_get_kill_kill12(void) { return (void *)0; }
