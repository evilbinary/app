/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mem_mem_mem02
#include "ltp/testcases/kernel/mem/mem/mem02.c"
#undef main
int LTP_OLD_MAIN_mem_mem_mem02();
int ltp_run_mem_mem_mem02(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mem_mem_mem02)(argc, argv);
}
struct tst_test *ltp_get_mem_mem_mem02(void);
struct tst_test *ltp_get_mem_mem_mem02(void) { return (void *)0; }
