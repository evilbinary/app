/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_nftw_nftw
#include "ltp/testcases/kernel/syscalls/nftw/nftw.c"
#undef main
int LTP_OLD_MAIN_nftw_nftw();
int ltp_run_nftw_nftw(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_nftw_nftw)(argc, argv);
}
struct tst_test *ltp_get_nftw_nftw(void);
struct tst_test *ltp_get_nftw_nftw(void) { return (void *)0; }
