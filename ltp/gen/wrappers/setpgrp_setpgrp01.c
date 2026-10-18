/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_setpgrp_setpgrp01
#include "ltp/testcases/kernel/syscalls/setpgrp/setpgrp01.c"
#undef main
int LTP_OLD_MAIN_setpgrp_setpgrp01();
int ltp_run_setpgrp_setpgrp01(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_setpgrp_setpgrp01)(argc, argv);
}
struct tst_test *ltp_get_setpgrp_setpgrp01(void);
struct tst_test *ltp_get_setpgrp_setpgrp01(void) { return (void *)0; }
