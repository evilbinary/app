/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_security_filecaps_check_simple_capset
#include "ltp/testcases/kernel/security/filecaps/check_simple_capset.c"
#undef main
int LTP_OLD_MAIN_security_filecaps_check_simple_capset();
int ltp_run_security_filecaps_check_simple_capset(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_security_filecaps_check_simple_capset)(argc, argv);
}
struct tst_test *ltp_get_security_filecaps_check_simple_capset(void);
struct tst_test *ltp_get_security_filecaps_check_simple_capset(void) { return (void *)0; }
