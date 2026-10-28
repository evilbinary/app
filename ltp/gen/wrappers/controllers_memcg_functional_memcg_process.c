/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_memcg_functional_memcg_process
#include "ltp/testcases/kernel/controllers/memcg/functional/memcg_process.c"
#undef main
int LTP_OLD_MAIN_controllers_memcg_functional_memcg_process();
int ltp_run_controllers_memcg_functional_memcg_process(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_memcg_functional_memcg_process)(argc, argv);
}
struct tst_test *ltp_get_controllers_memcg_functional_memcg_process(void);
struct tst_test *ltp_get_controllers_memcg_functional_memcg_process(void) { return (void *)0; }
