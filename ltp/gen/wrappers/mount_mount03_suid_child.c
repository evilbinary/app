/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mount_mount03_suid_child
#include "ltp/testcases/kernel/syscalls/mount/mount03_suid_child.c"
#undef main
int LTP_OLD_MAIN_mount_mount03_suid_child();
int ltp_run_mount_mount03_suid_child(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mount_mount03_suid_child)(argc, argv);
}
struct tst_test *ltp_get_mount_mount03_suid_child(void);
struct tst_test *ltp_get_mount_mount03_suid_child(void) { return (void *)0; }
