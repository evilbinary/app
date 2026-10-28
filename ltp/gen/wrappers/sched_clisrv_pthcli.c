/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_sched_clisrv_pthcli
#include "ltp/testcases/kernel/sched/clisrv/pthcli.c"
#undef main
int LTP_OLD_MAIN_sched_clisrv_pthcli();
int ltp_run_sched_clisrv_pthcli(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_sched_clisrv_pthcli)(argc, argv);
}
struct tst_test *ltp_get_sched_clisrv_pthcli(void);
struct tst_test *ltp_get_sched_clisrv_pthcli(void) { return (void *)0; }
