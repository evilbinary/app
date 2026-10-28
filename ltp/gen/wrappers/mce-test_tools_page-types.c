/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mce-test_tools_page-types
#include "ltp/testcases/kernel/mce-test/tools/page-types.c"
#undef main
int LTP_OLD_MAIN_mce-test_tools_page-types();
int ltp_run_mce-test_tools_page-types(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mce-test_tools_page-types)(argc, argv);
}
struct tst_test *ltp_get_mce-test_tools_page-types(void);
struct tst_test *ltp_get_mce-test_tools_page-types(void) { return (void *)0; }
