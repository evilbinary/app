/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mem_mtest07_shm_test
#include "ltp/testcases/kernel/mem/mtest07/shm_test.c"
#undef main
int LTP_OLD_MAIN_mem_mtest07_shm_test();
int ltp_run_mem_mtest07_shm_test(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mem_mtest07_shm_test)(argc, argv);
}
struct tst_test *ltp_get_mem_mtest07_shm_test(void);
struct tst_test *ltp_get_mem_mtest07_shm_test(void) { return (void *)0; }
