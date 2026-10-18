/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_sockioctl_sockioctl01
#include "ltp/testcases/kernel/syscalls/sockioctl/sockioctl01.c"
#undef main
int LTP_OLD_MAIN_sockioctl_sockioctl01();
int ltp_run_sockioctl_sockioctl01(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_sockioctl_sockioctl01)(argc, argv);
}
struct tst_test *ltp_get_sockioctl_sockioctl01(void);
struct tst_test *ltp_get_sockioctl_sockioctl01(void) { return (void *)0; }
