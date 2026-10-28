/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_connectors_pec_pec_listener
#include "ltp/testcases/kernel/connectors/pec/pec_listener.c"
#undef main
int LTP_OLD_MAIN_connectors_pec_pec_listener();
int ltp_run_connectors_pec_pec_listener(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_connectors_pec_pec_listener)(argc, argv);
}
struct tst_test *ltp_get_connectors_pec_pec_listener(void);
struct tst_test *ltp_get_connectors_pec_pec_listener(void) { return (void *)0; }
