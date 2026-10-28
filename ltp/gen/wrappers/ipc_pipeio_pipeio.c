/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_ipc_pipeio_pipeio
#include "ltp/testcases/kernel/ipc/pipeio/pipeio.c"
#undef main
int LTP_OLD_MAIN_ipc_pipeio_pipeio();
int ltp_run_ipc_pipeio_pipeio(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_ipc_pipeio_pipeio)(argc, argv);
}
struct tst_test *ltp_get_ipc_pipeio_pipeio(void);
struct tst_test *ltp_get_ipc_pipeio_pipeio(void) { return (void *)0; }
