/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_mce-test_stress_tools_fs-metadata_k-tree-gen
#include "ltp/testcases/kernel/mce-test/stress/tools/fs-metadata/k-tree-gen.c"
#undef main
int LTP_OLD_MAIN_mce-test_stress_tools_fs-metadata_k-tree-gen();
int ltp_run_mce-test_stress_tools_fs-metadata_k-tree-gen(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_mce-test_stress_tools_fs-metadata_k-tree-gen)(argc, argv);
}
struct tst_test *ltp_get_mce-test_stress_tools_fs-metadata_k-tree-gen(void);
struct tst_test *ltp_get_mce-test_stress_tools_fs-metadata_k-tree-gen(void) { return (void *)0; }
