/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mce-test_tsrc_tsoftinj
#include "ltp/testcases/kernel/mce-test/tsrc/tsoftinj.c"
#undef main
int LTP_OLD_MAIN_mce-test_tsrc_tsoftinj();
int ltp_run_mce-test_tsrc_tsoftinj(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mce-test_tsrc_tsoftinj)(argc, argv);
}
struct tst_test *ltp_get_mce-test_tsrc_tsoftinj(void);
struct tst_test *ltp_get_mce-test_tsrc_tsoftinj(void) { return (void *)0; }
