/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_sched_tool_trace_sched
#include "ltp/testcases/kernel/sched/tool/trace_sched.c"
#undef main
int LTP_OLD_MAIN_sched_tool_trace_sched();
int ltp_run_sched_tool_trace_sched(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_sched_tool_trace_sched)(argc, argv);
}
struct tst_test *ltp_get_sched_tool_trace_sched(void);
struct tst_test *ltp_get_sched_tool_trace_sched(void) { return (void *)0; }
