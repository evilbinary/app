/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_sched_hyperthreading_ht_enabled_ht_enabled
#include "ltp/testcases/kernel/sched/hyperthreading/ht_enabled/ht_enabled.c"
#undef main
int LTP_OLD_MAIN_sched_hyperthreading_ht_enabled_ht_enabled();
int ltp_run_sched_hyperthreading_ht_enabled_ht_enabled(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_sched_hyperthreading_ht_enabled_ht_enabled)(argc, argv);
}
struct tst_test *ltp_get_sched_hyperthreading_ht_enabled_ht_enabled(void);
struct tst_test *ltp_get_sched_hyperthreading_ht_enabled_ht_enabled(void) { return (void *)0; }
