/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_fs_doio_datapid
#include "ltp/testcases/kernel/fs/doio/datapid.c"
#undef main
int LTP_OLD_MAIN_fs_doio_datapid();
int ltp_run_fs_doio_datapid(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_fs_doio_datapid)(argc, argv);
}
struct tst_test *ltp_get_fs_doio_datapid(void);
struct tst_test *ltp_get_fs_doio_datapid(void) { return (void *)0; }
