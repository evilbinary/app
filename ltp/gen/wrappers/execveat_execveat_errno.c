/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_execveat_execveat_errno
#include "ltp/testcases/kernel/syscalls/execveat/execveat_errno.c"
#undef main
int LTP_OLD_MAIN_execveat_execveat_errno();
int ltp_run_execveat_execveat_errno(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_execveat_execveat_errno)(argc, argv);
}
struct tst_test *ltp_get_execveat_execveat_errno(void);
struct tst_test *ltp_get_execveat_execveat_errno(void) { return (void *)0; }
