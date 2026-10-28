/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mce_test_tsrc_tsimpleinj
#include "ltp/testcases/kernel/mce-test/tsrc/tsimpleinj.c"
#undef main
int LTP_OLD_MAIN_mce_test_tsrc_tsimpleinj();
int ltp_run_mce_test_tsrc_tsimpleinj(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mce_test_tsrc_tsimpleinj)(argc, argv);
}
struct tst_test *ltp_get_mce_test_tsrc_tsimpleinj(void);
struct tst_test *ltp_get_mce_test_tsrc_tsimpleinj(void) { return (void *)0; }
