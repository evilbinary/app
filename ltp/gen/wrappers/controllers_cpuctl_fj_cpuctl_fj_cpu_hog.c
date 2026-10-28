/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_cpuctl_fj_cpuctl_fj_cpu_hog
#include "ltp/testcases/kernel/controllers/cpuctl_fj/cpuctl_fj_cpu-hog.c"
#undef main
int LTP_OLD_MAIN_controllers_cpuctl_fj_cpuctl_fj_cpu_hog();
int ltp_run_controllers_cpuctl_fj_cpuctl_fj_cpu_hog(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_cpuctl_fj_cpuctl_fj_cpu_hog)(argc, argv);
}
struct tst_test *ltp_get_controllers_cpuctl_fj_cpuctl_fj_cpu_hog(void);
struct tst_test *ltp_get_controllers_cpuctl_fj_cpuctl_fj_cpu_hog(void) { return (void *)0; }
