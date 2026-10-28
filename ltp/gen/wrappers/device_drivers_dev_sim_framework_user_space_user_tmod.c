/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_device_drivers_dev_sim_framework_user_space_user_tmod
#include "ltp/testcases/kernel/device-drivers/dev_sim_framework/user_space/user_tmod.c"
#undef main
int LTP_OLD_MAIN_device_drivers_dev_sim_framework_user_space_user_tmod();
int ltp_run_device_drivers_dev_sim_framework_user_space_user_tmod(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_device_drivers_dev_sim_framework_user_space_user_tmod)(argc, argv);
}
struct tst_test *ltp_get_device_drivers_dev_sim_framework_user_space_user_tmod(void);
struct tst_test *ltp_get_device_drivers_dev_sim_framework_user_space_user_tmod(void) { return (void *)0; }
