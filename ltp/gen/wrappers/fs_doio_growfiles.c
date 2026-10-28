/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_fs_doio_growfiles
#include "ltp/testcases/kernel/fs/doio/growfiles.c"
#undef main
int LTP_OLD_MAIN_fs_doio_growfiles();
int ltp_run_fs_doio_growfiles(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_fs_doio_growfiles)(argc, argv);
}
struct tst_test *ltp_get_fs_doio_growfiles(void);
struct tst_test *ltp_get_fs_doio_growfiles(void) { return (void *)0; }
