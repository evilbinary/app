/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_device_drivers_nls_userBlockNLS
#include "ltp/testcases/kernel/device-drivers/nls/userBlockNLS.c"
#undef main
int LTP_OLD_MAIN_device_drivers_nls_userBlockNLS();
int ltp_run_device_drivers_nls_userBlockNLS(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_device_drivers_nls_userBlockNLS)(argc, argv);
}
struct tst_test *ltp_get_device_drivers_nls_userBlockNLS(void);
struct tst_test *ltp_get_device_drivers_nls_userBlockNLS(void) { return (void *)0; }
