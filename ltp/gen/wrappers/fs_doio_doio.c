/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_fs_doio_doio
#include "ltp/testcases/kernel/fs/doio/doio.c"
#undef main
int LTP_OLD_MAIN_fs_doio_doio();
int ltp_run_fs_doio_doio(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_fs_doio_doio)(argc, argv);
}
struct tst_test *ltp_get_fs_doio_doio(void);
struct tst_test *ltp_get_fs_doio_doio(void) { return (void *)0; }
