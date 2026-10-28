/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_security_prot_hsymlinks_prot_hsymlinks
#include "ltp/testcases/kernel/security/prot_hsymlinks/prot_hsymlinks.c"
#undef main
int LTP_OLD_MAIN_security_prot_hsymlinks_prot_hsymlinks();
int ltp_run_security_prot_hsymlinks_prot_hsymlinks(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_security_prot_hsymlinks_prot_hsymlinks)(argc, argv);
}
struct tst_test *ltp_get_security_prot_hsymlinks_prot_hsymlinks(void);
struct tst_test *ltp_get_security_prot_hsymlinks_prot_hsymlinks(void) { return (void *)0; }
