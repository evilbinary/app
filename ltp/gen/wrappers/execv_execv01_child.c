/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_execv_execv01_child
#include "ltp/testcases/kernel/syscalls/execv/execv01_child.c"
#undef main
int LTP_OLD_MAIN_execv_execv01_child();
int ltp_run_execv_execv01_child(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_execv_execv01_child)(argc, argv);
}
struct tst_test *ltp_get_execv_execv01_child(void);
struct tst_test *ltp_get_execv_execv01_child(void) { return (void *)0; }
