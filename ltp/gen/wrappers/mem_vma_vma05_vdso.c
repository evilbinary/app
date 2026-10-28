/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mem_vma_vma05_vdso
#include "ltp/testcases/kernel/mem/vma/vma05_vdso.c"
#undef main
int LTP_OLD_MAIN_mem_vma_vma05_vdso();
int ltp_run_mem_vma_vma05_vdso(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mem_vma_vma05_vdso)(argc, argv);
}
struct tst_test *ltp_get_mem_vma_vma05_vdso(void);
struct tst_test *ltp_get_mem_vma_vma05_vdso(void) { return (void *)0; }
