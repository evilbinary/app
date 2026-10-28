/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_device-drivers_agp_user_space_user_tagp
#include "ltp/testcases/kernel/device-drivers/agp/user_space/user_tagp.c"
#undef main
int LTP_OLD_MAIN_device-drivers_agp_user_space_user_tagp();
int ltp_run_device-drivers_agp_user_space_user_tagp(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_device-drivers_agp_user_space_user_tagp)(argc, argv);
}
struct tst_test *ltp_get_device-drivers_agp_user_space_user_tagp(void);
struct tst_test *ltp_get_device-drivers_agp_user_space_user_tagp(void) { return (void *)0; }
