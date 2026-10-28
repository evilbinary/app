/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_device-drivers_base_user_base_user_tbase
#include "ltp/testcases/kernel/device-drivers/base/user_base/user_tbase.c"
#undef main
int LTP_OLD_MAIN_device-drivers_base_user_base_user_tbase();
int ltp_run_device-drivers_base_user_base_user_tbase(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_device-drivers_base_user_base_user_tbase)(argc, argv);
}
struct tst_test *ltp_get_device-drivers_base_user_base_user_tbase(void);
struct tst_test *ltp_get_device-drivers_base_user_base_user_tbase(void) { return (void *)0; }
