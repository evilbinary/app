/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_fs_fs_di_create_datafile
#include "ltp/testcases/kernel/fs/fs_di/create_datafile.c"
#undef main
int LTP_OLD_MAIN_fs_fs_di_create_datafile();
int ltp_run_fs_fs_di_create_datafile(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_fs_fs_di_create_datafile)(argc, argv);
}
struct tst_test *ltp_get_fs_fs_di_create_datafile(void);
struct tst_test *ltp_get_fs_fs_di_create_datafile(void) { return (void *)0; }
