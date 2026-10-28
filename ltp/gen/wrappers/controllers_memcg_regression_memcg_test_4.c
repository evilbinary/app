/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_memcg_regression_memcg_test_4
#include "ltp/testcases/kernel/controllers/memcg/regression/memcg_test_4.c"
#undef main
int LTP_OLD_MAIN_controllers_memcg_regression_memcg_test_4();
int ltp_run_controllers_memcg_regression_memcg_test_4(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_memcg_regression_memcg_test_4)(argc, argv);
}
struct tst_test *ltp_get_controllers_memcg_regression_memcg_test_4(void);
struct tst_test *ltp_get_controllers_memcg_regression_memcg_test_4(void) { return (void *)0; }
