/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mce_test_tsrc_erst_inj_erst_inject
#include "ltp/testcases/kernel/mce-test/tsrc/erst-inj/erst-inject.c"
#undef main
int LTP_OLD_MAIN_mce_test_tsrc_erst_inj_erst_inject();
int ltp_run_mce_test_tsrc_erst_inj_erst_inject(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mce_test_tsrc_erst_inj_erst_inject)(argc, argv);
}
struct tst_test *ltp_get_mce_test_tsrc_erst_inj_erst_inject(void);
struct tst_test *ltp_get_mce_test_tsrc_erst_inj_erst_inject(void) { return (void *)0; }
