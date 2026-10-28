/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mem_mmapstress_mmapstress09
#include "ltp/testcases/kernel/mem/mmapstress/mmapstress09.c"
#undef main
int LTP_OLD_MAIN_mem_mmapstress_mmapstress09();
int ltp_run_mem_mmapstress_mmapstress09(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mem_mmapstress_mmapstress09)(argc, argv);
}
struct tst_test *ltp_get_mem_mmapstress_mmapstress09(void);
struct tst_test *ltp_get_mem_mmapstress_mmapstress09(void) { return (void *)0; }
