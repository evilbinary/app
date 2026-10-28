/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_security_cap_bound_cap_bounds_r
#include "ltp/testcases/kernel/security/cap_bound/cap_bounds_r.c"
#undef main
int LTP_OLD_MAIN_security_cap_bound_cap_bounds_r();
int ltp_run_security_cap_bound_cap_bounds_r(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_security_cap_bound_cap_bounds_r)(argc, argv);
}
struct tst_test *ltp_get_security_cap_bound_cap_bounds_r(void);
struct tst_test *ltp_get_security_cap_bound_cap_bounds_r(void) { return (void *)0; }
