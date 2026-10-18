/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_remap_file_pages_remap_file_pages01
#include "ltp/testcases/kernel/syscalls/remap_file_pages/remap_file_pages01.c"
#undef main
int LTP_OLD_MAIN_remap_file_pages_remap_file_pages01();
int ltp_run_remap_file_pages_remap_file_pages01(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_remap_file_pages_remap_file_pages01)(argc, argv);
}
struct tst_test *ltp_get_remap_file_pages_remap_file_pages01(void);
struct tst_test *ltp_get_remap_file_pages_remap_file_pages01(void) { return (void *)0; }
