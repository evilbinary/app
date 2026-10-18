/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_renameat2_renameat201
#include "ltp/testcases/kernel/syscalls/renameat2/renameat201.c"
#undef main
int LTP_OLD_MAIN_renameat2_renameat201();
int ltp_run_renameat2_renameat201(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_renameat2_renameat201)(argc, argv);
}
struct tst_test *ltp_get_renameat2_renameat201(void);
struct tst_test *ltp_get_renameat2_renameat201(void) { return (void *)0; }
