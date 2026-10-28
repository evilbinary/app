/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mem_mtest05_mmstress_dummy
#include "ltp/testcases/kernel/mem/mtest05/mmstress_dummy.c"
#undef main
int LTP_OLD_MAIN_mem_mtest05_mmstress_dummy();
int ltp_run_mem_mtest05_mmstress_dummy(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mem_mtest05_mmstress_dummy)(argc, argv);
}
struct tst_test *ltp_get_mem_mtest05_mmstress_dummy(void);
struct tst_test *ltp_get_mem_mtest05_mmstress_dummy(void) { return (void *)0; }
