/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_sched_hyperthreading_ht_affinity_ht_affinity
#include "ltp/testcases/kernel/sched/hyperthreading/ht_affinity/ht_affinity.c"
#undef main
int LTP_OLD_MAIN_sched_hyperthreading_ht_affinity_ht_affinity();
int ltp_run_sched_hyperthreading_ht_affinity_ht_affinity(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_sched_hyperthreading_ht_affinity_ht_affinity)(argc, argv);
}
struct tst_test *ltp_get_sched_hyperthreading_ht_affinity_ht_affinity(void);
struct tst_test *ltp_get_sched_hyperthreading_ht_affinity_ht_affinity(void) { return (void *)0; }
