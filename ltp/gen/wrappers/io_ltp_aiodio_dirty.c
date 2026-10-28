/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_io_ltp_aiodio_dirty
#include "ltp/testcases/kernel/io/ltp-aiodio/dirty.c"
#undef main
int LTP_OLD_MAIN_io_ltp_aiodio_dirty();
int ltp_run_io_ltp_aiodio_dirty(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_io_ltp_aiodio_dirty)(argc, argv);
}
struct tst_test *ltp_get_io_ltp_aiodio_dirty(void);
struct tst_test *ltp_get_io_ltp_aiodio_dirty(void) { return (void *)0; }
