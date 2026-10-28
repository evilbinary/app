/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_security_filecaps_verify_caps_exec
#include "ltp/testcases/kernel/security/filecaps/verify_caps_exec.c"
#undef main
int LTP_OLD_MAIN_security_filecaps_verify_caps_exec();
int ltp_run_security_filecaps_verify_caps_exec(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_security_filecaps_verify_caps_exec)(argc, argv);
}
struct tst_test *ltp_get_security_filecaps_verify_caps_exec(void);
struct tst_test *ltp_get_security_filecaps_verify_caps_exec(void) { return (void *)0; }
