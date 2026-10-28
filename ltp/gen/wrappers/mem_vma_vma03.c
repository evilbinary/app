/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mem_vma_vma03
#include "ltp/testcases/kernel/mem/vma/vma03.c"
#undef main
int LTP_OLD_MAIN_mem_vma_vma03();
int ltp_run_mem_vma_vma03(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mem_vma_vma03)(argc, argv);
}
struct tst_test *ltp_get_mem_vma_vma03(void);
struct tst_test *ltp_get_mem_vma_vma03(void) { return (void *)0; }
