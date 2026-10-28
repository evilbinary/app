/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mce_test_tsrc_tinjpage
#include "ltp/testcases/kernel/mce-test/tsrc/tinjpage.c"
#undef main
int LTP_OLD_MAIN_mce_test_tsrc_tinjpage();
int ltp_run_mce_test_tsrc_tinjpage(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mce_test_tsrc_tinjpage)(argc, argv);
}
struct tst_test *ltp_get_mce_test_tsrc_tinjpage(void);
struct tst_test *ltp_get_mce_test_tsrc_tinjpage(void) { return (void *)0; }
