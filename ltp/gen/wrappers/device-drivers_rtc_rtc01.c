/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_device-drivers_rtc_rtc01
#include "ltp/testcases/kernel/device-drivers/rtc/rtc01.c"
#undef main
int LTP_OLD_MAIN_device-drivers_rtc_rtc01();
int ltp_run_device-drivers_rtc_rtc01(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_device-drivers_rtc_rtc01)(argc, argv);
}
struct tst_test *ltp_get_device-drivers_rtc_rtc01(void);
struct tst_test *ltp_get_device-drivers_rtc_rtc01(void) { return (void *)0; }
