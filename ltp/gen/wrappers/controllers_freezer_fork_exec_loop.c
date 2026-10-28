/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_freezer_fork_exec_loop
#include "ltp/testcases/kernel/controllers/freezer/fork_exec_loop.c"
#undef main
int LTP_OLD_MAIN_controllers_freezer_fork_exec_loop();
int ltp_run_controllers_freezer_fork_exec_loop(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_freezer_fork_exec_loop)(argc, argv);
}
struct tst_test *ltp_get_controllers_freezer_fork_exec_loop(void);
struct tst_test *ltp_get_controllers_freezer_fork_exec_loop(void) { return (void *)0; }
