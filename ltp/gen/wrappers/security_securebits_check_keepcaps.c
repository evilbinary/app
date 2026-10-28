/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_security_securebits_check_keepcaps
#include "ltp/testcases/kernel/security/securebits/check_keepcaps.c"
#undef main
int LTP_OLD_MAIN_security_securebits_check_keepcaps();
int ltp_run_security_securebits_check_keepcaps(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_security_securebits_check_keepcaps)(argc, argv);
}
struct tst_test *ltp_get_security_securebits_check_keepcaps(void);
struct tst_test *ltp_get_security_securebits_check_keepcaps(void) { return (void *)0; }
