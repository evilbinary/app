/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_fs_fs_perms_fs_perms
#include "ltp/testcases/kernel/fs/fs_perms/fs_perms.c"
#undef main
int LTP_OLD_MAIN_fs_fs_perms_fs_perms();
int ltp_run_fs_fs_perms_fs_perms(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_fs_fs_perms_fs_perms)(argc, argv);
}
struct tst_test *ltp_get_fs_fs_perms_fs_perms(void);
struct tst_test *ltp_get_fs_fs_perms_fs_perms(void) { return (void *)0; }
