/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_device_drivers_usb_user_usb_user_tusb
#include "ltp/testcases/kernel/device-drivers/usb/user_usb/user_tusb.c"
#undef main
int LTP_OLD_MAIN_device_drivers_usb_user_usb_user_tusb();
int ltp_run_device_drivers_usb_user_usb_user_tusb(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_device_drivers_usb_user_usb_user_tusb)(argc, argv);
}
struct tst_test *ltp_get_device_drivers_usb_user_usb_user_tusb(void);
struct tst_test *ltp_get_device_drivers_usb_user_usb_user_tusb(void) { return (void *)0; }
