/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_zrepro_zrepro01
#include "ltp/testcases/kernel/syscalls/zrepro/zrepro01.c"
#undef main
int LTP_OLD_MAIN_zrepro_zrepro01();
int ltp_run_zrepro_zrepro01(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_zrepro_zrepro01)(argc, argv);
}
struct tst_test *ltp_get_zrepro_zrepro01(void);
struct tst_test *ltp_get_zrepro_zrepro01(void) { return (void *)0; }
