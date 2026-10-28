/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_fs_fsstress_fsstress
#include "ltp/testcases/kernel/fs/fsstress/fsstress.c"
#undef main
int LTP_OLD_MAIN_fs_fsstress_fsstress();
int ltp_run_fs_fsstress_fsstress(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_fs_fsstress_fsstress)(argc, argv);
}
struct tst_test *ltp_get_fs_fsstress_fsstress(void);
struct tst_test *ltp_get_fs_fsstress_fsstress(void) { return (void *)0; }
