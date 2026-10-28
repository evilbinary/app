/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mem_vmtests_data_space
#include "ltp/testcases/kernel/mem/vmtests/data_space.c"
#undef main
int LTP_OLD_MAIN_mem_vmtests_data_space();
int ltp_run_mem_vmtests_data_space(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mem_vmtests_data_space)(argc, argv);
}
struct tst_test *ltp_get_mem_vmtests_data_space(void);
struct tst_test *ltp_get_mem_vmtests_data_space(void) { return (void *)0; }
