/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_controllers_cpuset_cpuset_syscall_test_cpuset_syscall_test
#include "ltp/testcases/kernel/controllers/cpuset/cpuset_syscall_test/cpuset_syscall_test.c"
#undef main
int LTP_OLD_MAIN_controllers_cpuset_cpuset_syscall_test_cpuset_syscall_test();
int ltp_run_controllers_cpuset_cpuset_syscall_test_cpuset_syscall_test(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_controllers_cpuset_cpuset_syscall_test_cpuset_syscall_test)(argc, argv);
}
struct tst_test *ltp_get_controllers_cpuset_cpuset_syscall_test_cpuset_syscall_test(void);
struct tst_test *ltp_get_controllers_cpuset_cpuset_syscall_test_cpuset_syscall_test(void) { return (void *)0; }
