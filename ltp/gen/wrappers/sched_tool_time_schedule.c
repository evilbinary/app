/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_sched_tool_time_schedule
#include "ltp/testcases/kernel/sched/tool/time-schedule.c"
#undef main
int LTP_OLD_MAIN_sched_tool_time_schedule();
int ltp_run_sched_tool_time_schedule(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_sched_tool_time_schedule)(argc, argv);
}
struct tst_test *ltp_get_sched_tool_time_schedule(void);
struct tst_test *ltp_get_sched_tool_time_schedule(void) { return (void *)0; }
