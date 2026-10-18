/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_renameat2_renameat202
#include "ltp/testcases/kernel/syscalls/renameat2/renameat202.c"
#undef main
int LTP_OLD_MAIN_renameat2_renameat202();
int ltp_run_renameat2_renameat202(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_renameat2_renameat202)(argc, argv);
}
struct tst_test *ltp_get_renameat2_renameat202(void);
struct tst_test *ltp_get_renameat2_renameat202(void) { return (void *)0; }
