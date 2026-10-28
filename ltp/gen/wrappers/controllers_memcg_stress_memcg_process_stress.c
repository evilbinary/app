/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_memcg_stress_memcg_process_stress
#include "ltp/testcases/kernel/controllers/memcg/stress/memcg_process_stress.c"
#undef main
int LTP_OLD_MAIN_controllers_memcg_stress_memcg_process_stress();
int ltp_run_controllers_memcg_stress_memcg_process_stress(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_memcg_stress_memcg_process_stress)(argc, argv);
}
struct tst_test *ltp_get_controllers_memcg_stress_memcg_process_stress(void);
struct tst_test *ltp_get_controllers_memcg_stress_memcg_process_stress(void) { return (void *)0; }
