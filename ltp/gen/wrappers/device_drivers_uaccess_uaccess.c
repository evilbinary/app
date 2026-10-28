/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_device_drivers_uaccess_uaccess
#include "ltp/testcases/kernel/device-drivers/uaccess/uaccess.c"
#undef main
int LTP_OLD_MAIN_device_drivers_uaccess_uaccess();
int ltp_run_device_drivers_uaccess_uaccess(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_device_drivers_uaccess_uaccess)(argc, argv);
}
struct tst_test *ltp_get_device_drivers_uaccess_uaccess(void);
struct tst_test *ltp_get_device_drivers_uaccess_uaccess(void) { return (void *)0; }
