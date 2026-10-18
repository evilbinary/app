/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_rename_rename11
#include "ltp/testcases/kernel/syscalls/rename/rename11.c"
#undef main
int LTP_OLD_MAIN_rename_rename11();
int ltp_run_rename_rename11(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_rename_rename11)(argc, argv);
}
struct tst_test *ltp_get_rename_rename11(void);
struct tst_test *ltp_get_rename_rename11(void) { return (void *)0; }
