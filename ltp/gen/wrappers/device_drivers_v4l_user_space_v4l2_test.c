/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_device_drivers_v4l_user_space_v4l2_test
#include "ltp/testcases/kernel/device-drivers/v4l/user_space/v4l2_test.c"
#undef main
int LTP_OLD_MAIN_device_drivers_v4l_user_space_v4l2_test();
int ltp_run_device_drivers_v4l_user_space_v4l2_test(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_device_drivers_v4l_user_space_v4l2_test)(argc, argv);
}
struct tst_test *ltp_get_device_drivers_v4l_user_space_v4l2_test(void);
struct tst_test *ltp_get_device_drivers_v4l_user_space_v4l2_test(void) { return (void *)0; }
