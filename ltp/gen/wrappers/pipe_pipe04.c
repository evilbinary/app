/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_pipe_pipe04
#include "ltp/testcases/kernel/syscalls/pipe/pipe04.c"
#undef main
int LTP_OLD_MAIN_pipe_pipe04();
int ltp_run_pipe_pipe04(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_pipe_pipe04)(argc, argv);
}
struct tst_test *ltp_get_pipe_pipe04(void);
struct tst_test *ltp_get_pipe_pipe04(void) { return (void *)0; }
