/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_security_smack_smack_set_socket_labels
#include "ltp/testcases/kernel/security/smack/smack_set_socket_labels.c"
#undef main
int LTP_OLD_MAIN_security_smack_smack_set_socket_labels();
int ltp_run_security_smack_smack_set_socket_labels(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_security_smack_smack_set_socket_labels)(argc, argv);
}
struct tst_test *ltp_get_security_smack_smack_set_socket_labels(void);
struct tst_test *ltp_get_security_smack_smack_set_socket_labels(void) { return (void *)0; }
