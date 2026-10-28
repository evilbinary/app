/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_pids_pids_task1
#include "ltp/testcases/kernel/controllers/pids/pids_task1.c"
#undef main
int LTP_OLD_MAIN_controllers_pids_pids_task1();
int ltp_run_controllers_pids_pids_task1(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_pids_pids_task1)(argc, argv);
}
struct tst_test *ltp_get_controllers_pids_pids_task1(void);
struct tst_test *ltp_get_controllers_pids_pids_task1(void) { return (void *)0; }
