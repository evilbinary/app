/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_open_open12_child
#include "ltp/testcases/kernel/syscalls/open/open12_child.c"
#undef main
int LTP_OLD_MAIN_open_open12_child();
int ltp_run_open_open12_child(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_open_open12_child)(argc, argv);
}
struct tst_test *ltp_get_open_open12_child(void);
struct tst_test *ltp_get_open_open12_child(void) { return (void *)0; }
