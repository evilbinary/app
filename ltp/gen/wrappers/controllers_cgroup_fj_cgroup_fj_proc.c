/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_cgroup_fj_cgroup_fj_proc
#include "ltp/testcases/kernel/controllers/cgroup_fj/cgroup_fj_proc.c"
#undef main
int LTP_OLD_MAIN_controllers_cgroup_fj_cgroup_fj_proc();
int ltp_run_controllers_cgroup_fj_cgroup_fj_proc(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_cgroup_fj_cgroup_fj_proc)(argc, argv);
}
struct tst_test *ltp_get_controllers_cgroup_fj_cgroup_fj_proc(void);
struct tst_test *ltp_get_controllers_cgroup_fj_cgroup_fj_proc(void) { return (void *)0; }
