/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_cpuset_cpuset_memory_pressure_test_cpuset_memory_pressure
#include "ltp/testcases/kernel/controllers/cpuset/cpuset_memory_pressure_test/cpuset_memory_pressure.c"
#undef main
int LTP_OLD_MAIN_controllers_cpuset_cpuset_memory_pressure_test_cpuset_memory_pressure();
int ltp_run_controllers_cpuset_cpuset_memory_pressure_test_cpuset_memory_pressure(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_cpuset_cpuset_memory_pressure_test_cpuset_memory_pressure)(argc, argv);
}
struct tst_test *ltp_get_controllers_cpuset_cpuset_memory_pressure_test_cpuset_memory_pressure(void);
struct tst_test *ltp_get_controllers_cpuset_cpuset_memory_pressure_test_cpuset_memory_pressure(void) { return (void *)0; }
