/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_device_drivers_cpufreq_cpufreq_boost
#include "ltp/testcases/kernel/device-drivers/cpufreq/cpufreq_boost.c"
#undef main
int LTP_OLD_MAIN_device_drivers_cpufreq_cpufreq_boost();
int ltp_run_device_drivers_cpufreq_cpufreq_boost(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_device_drivers_cpufreq_cpufreq_boost)(argc, argv);
}
struct tst_test *ltp_get_device_drivers_cpufreq_cpufreq_boost(void);
struct tst_test *ltp_get_device_drivers_cpufreq_cpufreq_boost(void) { return (void *)0; }
