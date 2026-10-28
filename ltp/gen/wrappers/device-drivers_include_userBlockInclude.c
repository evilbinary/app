/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_device-drivers_include_userBlockInclude
#include "ltp/testcases/kernel/device-drivers/include/userBlockInclude.c"
#undef main
int LTP_OLD_MAIN_device-drivers_include_userBlockInclude();
int ltp_run_device-drivers_include_userBlockInclude(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_device-drivers_include_userBlockInclude)(argc, argv);
}
struct tst_test *ltp_get_device-drivers_include_userBlockInclude(void);
struct tst_test *ltp_get_device-drivers_include_userBlockInclude(void) { return (void *)0; }
