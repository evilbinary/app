/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_cpuset_cpuset_load_balance_test_cpuset_sched_domains_check
#include "ltp/testcases/kernel/controllers/cpuset/cpuset_load_balance_test/cpuset_sched_domains_check.c"
#undef main
int LTP_OLD_MAIN_controllers_cpuset_cpuset_load_balance_test_cpuset_sched_domains_check();
int ltp_run_controllers_cpuset_cpuset_load_balance_test_cpuset_sched_domains_check(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_cpuset_cpuset_load_balance_test_cpuset_sched_domains_check)(argc, argv);
}
struct tst_test *ltp_get_controllers_cpuset_cpuset_load_balance_test_cpuset_sched_domains_check(void);
struct tst_test *ltp_get_controllers_cpuset_cpuset_load_balance_test_cpuset_sched_domains_check(void) { return (void *)0; }
