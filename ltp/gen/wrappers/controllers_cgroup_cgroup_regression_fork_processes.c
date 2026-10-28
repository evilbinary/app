/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_cgroup_cgroup_regression_fork_processes
#include "ltp/testcases/kernel/controllers/cgroup/cgroup_regression_fork_processes.c"
#undef main
int LTP_OLD_MAIN_controllers_cgroup_cgroup_regression_fork_processes();
int ltp_run_controllers_cgroup_cgroup_regression_fork_processes(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_cgroup_cgroup_regression_fork_processes)(argc, argv);
}
struct tst_test *ltp_get_controllers_cgroup_cgroup_regression_fork_processes(void);
struct tst_test *ltp_get_controllers_cgroup_cgroup_regression_fork_processes(void) { return (void *)0; }
