/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_security_cap_bound_cap_bset_inh_bounds
#include "ltp/testcases/kernel/security/cap_bound/cap_bset_inh_bounds.c"
#undef main
int LTP_OLD_MAIN_security_cap_bound_cap_bset_inh_bounds();
int ltp_run_security_cap_bound_cap_bset_inh_bounds(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_security_cap_bound_cap_bset_inh_bounds)(argc, argv);
}
struct tst_test *ltp_get_security_cap_bound_cap_bset_inh_bounds(void);
struct tst_test *ltp_get_security_cap_bound_cap_bset_inh_bounds(void) { return (void *)0; }
