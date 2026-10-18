/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_nftw_nftw64
#include "ltp/testcases/kernel/syscalls/nftw/nftw64.c"
#undef main
int LTP_OLD_MAIN_nftw_nftw64();
int ltp_run_nftw_nftw64(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_nftw_nftw64)(argc, argv);
}
struct tst_test *ltp_get_nftw_nftw64(void);
struct tst_test *ltp_get_nftw_nftw64(void) { return (void *)0; }
