/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_fs_fs_di_frag
#include "ltp/testcases/kernel/fs/fs_di/frag.c"
#undef main
int LTP_OLD_MAIN_fs_fs_di_frag();
int ltp_run_fs_fs_di_frag(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_fs_fs_di_frag)(argc, argv);
}
struct tst_test *ltp_get_fs_fs_di_frag(void);
struct tst_test *ltp_get_fs_fs_di_frag(void) { return (void *)0; }
