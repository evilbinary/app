/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_cpuset_cpuset_hotplug_test_cpuset_list_compute
#include "ltp/testcases/kernel/controllers/cpuset/cpuset_hotplug_test/cpuset_list_compute.c"
#undef main
int LTP_OLD_MAIN_controllers_cpuset_cpuset_hotplug_test_cpuset_list_compute();
int ltp_run_controllers_cpuset_cpuset_hotplug_test_cpuset_list_compute(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_cpuset_cpuset_hotplug_test_cpuset_list_compute)(argc, argv);
}
struct tst_test *ltp_get_controllers_cpuset_cpuset_hotplug_test_cpuset_list_compute(void);
struct tst_test *ltp_get_controllers_cpuset_cpuset_hotplug_test_cpuset_list_compute(void) { return (void *)0; }
