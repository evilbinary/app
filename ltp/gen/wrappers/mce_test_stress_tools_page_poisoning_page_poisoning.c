/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mce_test_stress_tools_page_poisoning_page_poisoning
#include "ltp/testcases/kernel/mce-test/stress/tools/page-poisoning/page-poisoning.c"
#undef main
int LTP_OLD_MAIN_mce_test_stress_tools_page_poisoning_page_poisoning();
int ltp_run_mce_test_stress_tools_page_poisoning_page_poisoning(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mce_test_stress_tools_page_poisoning_page_poisoning)(argc, argv);
}
struct tst_test *ltp_get_mce_test_stress_tools_page_poisoning_page_poisoning(void);
struct tst_test *ltp_get_mce_test_stress_tools_page_poisoning_page_poisoning(void) { return (void *)0; }
