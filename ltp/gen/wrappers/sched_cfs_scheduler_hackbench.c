/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_sched_cfs_scheduler_hackbench
#include "ltp/testcases/kernel/sched/cfs-scheduler/hackbench.c"
#undef main
int LTP_OLD_MAIN_sched_cfs_scheduler_hackbench();
int ltp_run_sched_cfs_scheduler_hackbench(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_sched_cfs_scheduler_hackbench)(argc, argv);
}
struct tst_test *ltp_get_sched_cfs_scheduler_hackbench(void);
struct tst_test *ltp_get_sched_cfs_scheduler_hackbench(void) { return (void *)0; }
