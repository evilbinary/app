/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_sched_clisrv_pthserv
#include "ltp/testcases/kernel/sched/clisrv/pthserv.c"
#undef main
int LTP_OLD_MAIN_sched_clisrv_pthserv();
int ltp_run_sched_clisrv_pthserv(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_sched_clisrv_pthserv)(argc, argv);
}
struct tst_test *ltp_get_sched_clisrv_pthserv(void);
struct tst_test *ltp_get_sched_clisrv_pthserv(void) { return (void *)0; }
