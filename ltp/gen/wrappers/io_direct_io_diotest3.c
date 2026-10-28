/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_io_direct_io_diotest3
#include "ltp/testcases/kernel/io/direct_io/diotest3.c"
#undef main
int LTP_OLD_MAIN_io_direct_io_diotest3();
int ltp_run_io_direct_io_diotest3(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_io_direct_io_diotest3)(argc, argv);
}
struct tst_test *ltp_get_io_direct_io_diotest3(void);
struct tst_test *ltp_get_io_direct_io_diotest3(void) { return (void *)0; }
