/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_string_string01
#include "ltp/testcases/kernel/syscalls/string/string01.c"
#undef main
int LTP_OLD_MAIN_string_string01();
int ltp_run_string_string01(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_string_string01)(argc, argv);
}
struct tst_test *ltp_get_string_string01(void);
struct tst_test *ltp_get_string_string01(void) { return (void *)0; }
