/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mce-test_stress_tools_page-poisoning_page-poisoning
#include "ltp/testcases/kernel/mce-test/stress/tools/page-poisoning/page-poisoning.c"
#undef main
int LTP_OLD_MAIN_mce-test_stress_tools_page-poisoning_page-poisoning();
int ltp_run_mce-test_stress_tools_page-poisoning_page-poisoning(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mce-test_stress_tools_page-poisoning_page-poisoning)(argc, argv);
}
struct tst_test *ltp_get_mce-test_stress_tools_page-poisoning_page-poisoning(void);
struct tst_test *ltp_get_mce-test_stress_tools_page-poisoning_page-poisoning(void) { return (void *)0; }
