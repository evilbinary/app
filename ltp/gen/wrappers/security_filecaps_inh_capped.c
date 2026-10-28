/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_security_filecaps_inh_capped
#include "ltp/testcases/kernel/security/filecaps/inh_capped.c"
#undef main
int LTP_OLD_MAIN_security_filecaps_inh_capped();
int ltp_run_security_filecaps_inh_capped(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_security_filecaps_inh_capped)(argc, argv);
}
struct tst_test *ltp_get_security_filecaps_inh_capped(void);
struct tst_test *ltp_get_security_filecaps_inh_capped(void) { return (void *)0; }
