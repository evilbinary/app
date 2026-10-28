/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_memcg_regression_memcg_test_1
#include "ltp/testcases/kernel/controllers/memcg/regression/memcg_test_1.c"
#undef main
int LTP_OLD_MAIN_controllers_memcg_regression_memcg_test_1();
int ltp_run_controllers_memcg_regression_memcg_test_1(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_memcg_regression_memcg_test_1)(argc, argv);
}
struct tst_test *ltp_get_controllers_memcg_regression_memcg_test_1(void);
struct tst_test *ltp_get_controllers_memcg_regression_memcg_test_1(void) { return (void *)0; }
