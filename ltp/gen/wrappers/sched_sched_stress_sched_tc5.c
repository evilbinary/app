/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_sched_sched_stress_sched_tc5
#include "ltp/testcases/kernel/sched/sched_stress/sched_tc5.c"
#undef main
int LTP_OLD_MAIN_sched_sched_stress_sched_tc5();
int ltp_run_sched_sched_stress_sched_tc5(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_sched_sched_stress_sched_tc5)(argc, argv);
}
struct tst_test *ltp_get_sched_sched_stress_sched_tc5(void);
struct tst_test *ltp_get_sched_sched_stress_sched_tc5(void) { return (void *)0; }
