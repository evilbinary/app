/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_containers_userns_userns06_capcheck
#include "ltp/testcases/kernel/containers/userns/userns06_capcheck.c"
#undef main
int LTP_OLD_MAIN_containers_userns_userns06_capcheck();
int ltp_run_containers_userns_userns06_capcheck(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_containers_userns_userns06_capcheck)(argc, argv);
}
struct tst_test *ltp_get_containers_userns_userns06_capcheck(void);
struct tst_test *ltp_get_containers_userns_userns06_capcheck(void) { return (void *)0; }
