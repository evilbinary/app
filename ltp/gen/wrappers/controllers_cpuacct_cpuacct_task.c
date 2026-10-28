/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_cpuacct_cpuacct_task
#include "ltp/testcases/kernel/controllers/cpuacct/cpuacct_task.c"
#undef main
int LTP_OLD_MAIN_controllers_cpuacct_cpuacct_task();
int ltp_run_controllers_cpuacct_cpuacct_task(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_cpuacct_cpuacct_task)(argc, argv);
}
struct tst_test *ltp_get_controllers_cpuacct_cpuacct_task(void);
struct tst_test *ltp_get_controllers_cpuacct_cpuacct_task(void) { return (void *)0; }
