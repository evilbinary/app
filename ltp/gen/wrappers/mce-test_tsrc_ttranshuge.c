/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mce-test_tsrc_ttranshuge
#include "ltp/testcases/kernel/mce-test/tsrc/ttranshuge.c"
#undef main
int LTP_OLD_MAIN_mce-test_tsrc_ttranshuge();
int ltp_run_mce-test_tsrc_ttranshuge(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mce-test_tsrc_ttranshuge)(argc, argv);
}
struct tst_test *ltp_get_mce-test_tsrc_ttranshuge(void);
struct tst_test *ltp_get_mce-test_tsrc_ttranshuge(void) { return (void *)0; }
