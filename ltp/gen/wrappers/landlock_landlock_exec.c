/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_landlock_landlock_exec
#include "ltp/testcases/kernel/syscalls/landlock/landlock_exec.c"
#undef main
int LTP_OLD_MAIN_landlock_landlock_exec();
int ltp_run_landlock_landlock_exec(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_landlock_landlock_exec)(argc, argv);
}
struct tst_test *ltp_get_landlock_landlock_exec(void);
struct tst_test *ltp_get_landlock_landlock_exec(void) { return (void *)0; }
