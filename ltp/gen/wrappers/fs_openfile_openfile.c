/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_fs_openfile_openfile
#include "ltp/testcases/kernel/fs/openfile/openfile.c"
#undef main
int LTP_OLD_MAIN_fs_openfile_openfile();
int ltp_run_fs_openfile_openfile(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_fs_openfile_openfile)(argc, argv);
}
struct tst_test *ltp_get_fs_openfile_openfile(void);
struct tst_test *ltp_get_fs_openfile_openfile(void) { return (void *)0; }
