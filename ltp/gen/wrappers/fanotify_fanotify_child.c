/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_fanotify_fanotify_child
#include "ltp/testcases/kernel/syscalls/fanotify/fanotify_child.c"
#undef main
int LTP_OLD_MAIN_fanotify_fanotify_child();
int ltp_run_fanotify_fanotify_child(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_fanotify_fanotify_child)(argc, argv);
}
struct tst_test *ltp_get_fanotify_fanotify_child(void);
struct tst_test *ltp_get_fanotify_fanotify_child(void) { return (void *)0; }
