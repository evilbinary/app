/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_freezer_timed_forkbomb
#include "ltp/testcases/kernel/controllers/freezer/timed_forkbomb.c"
#undef main
int LTP_OLD_MAIN_controllers_freezer_timed_forkbomb();
int ltp_run_controllers_freezer_timed_forkbomb(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_freezer_timed_forkbomb)(argc, argv);
}
struct tst_test *ltp_get_controllers_freezer_timed_forkbomb(void);
struct tst_test *ltp_get_controllers_freezer_timed_forkbomb(void) { return (void *)0; }
