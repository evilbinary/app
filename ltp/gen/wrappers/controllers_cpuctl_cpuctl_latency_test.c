/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_cpuctl_cpuctl_latency_test
#include "ltp/testcases/kernel/controllers/cpuctl/cpuctl_latency_test.c"
#undef main
int LTP_OLD_MAIN_controllers_cpuctl_cpuctl_latency_test();
int ltp_run_controllers_cpuctl_cpuctl_latency_test(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_cpuctl_cpuctl_latency_test)(argc, argv);
}
struct tst_test *ltp_get_controllers_cpuctl_cpuctl_latency_test(void);
struct tst_test *ltp_get_controllers_cpuctl_cpuctl_latency_test(void) { return (void *)0; }
