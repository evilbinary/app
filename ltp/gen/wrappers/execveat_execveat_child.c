/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_execveat_execveat_child
#include "ltp/testcases/kernel/syscalls/execveat/execveat_child.c"
#undef main
int LTP_OLD_MAIN_execveat_execveat_child();
int ltp_run_execveat_execveat_child(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_execveat_execveat_child)(argc, argv);
}
struct tst_test *ltp_get_execveat_execveat_child(void);
struct tst_test *ltp_get_execveat_execveat_child(void) { return (void *)0; }
