/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mce_test_tools_simple_process_simple_process
#include "ltp/testcases/kernel/mce-test/tools/simple_process/simple_process.c"
#undef main
int LTP_OLD_MAIN_mce_test_tools_simple_process_simple_process();
int ltp_run_mce_test_tools_simple_process_simple_process(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mce_test_tools_simple_process_simple_process)(argc, argv);
}
struct tst_test *ltp_get_mce_test_tools_simple_process_simple_process(void);
struct tst_test *ltp_get_mce_test_tools_simple_process_simple_process(void) { return (void *)0; }
