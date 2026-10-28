/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_security_cap_bound_check_pe
#include "ltp/testcases/kernel/security/cap_bound/check_pe.c"
#undef main
int LTP_OLD_MAIN_security_cap_bound_check_pe();
int ltp_run_security_cap_bound_check_pe(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_security_cap_bound_check_pe)(argc, argv);
}
struct tst_test *ltp_get_security_cap_bound_check_pe(void);
struct tst_test *ltp_get_security_cap_bound_check_pe(void) { return (void *)0; }
