/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_move_pages_move_pages05
#include "ltp/testcases/kernel/syscalls/move_pages/move_pages05.c"
#undef main
int LTP_OLD_MAIN_move_pages_move_pages05();
int ltp_run_move_pages_move_pages05(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_move_pages_move_pages05)(argc, argv);
}
struct tst_test *ltp_get_move_pages_move_pages05(void);
struct tst_test *ltp_get_move_pages_move_pages05(void) { return (void *)0; }
