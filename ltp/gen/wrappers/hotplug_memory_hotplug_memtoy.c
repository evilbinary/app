/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */
#define main LTP_OLD_MAIN_hotplug_memory_hotplug_memtoy
#include "ltp/testcases/kernel/hotplug/memory_hotplug/memtoy.c"
#undef main
int LTP_OLD_MAIN_hotplug_memory_hotplug_memtoy();
int ltp_run_hotplug_memory_hotplug_memtoy(int argc, char **argv) {
  return ((int (*)(int, char **))LTP_OLD_MAIN_hotplug_memory_hotplug_memtoy)(argc, argv);
}
struct tst_test *ltp_get_hotplug_memory_hotplug_memtoy(void);
struct tst_test *ltp_get_hotplug_memory_hotplug_memtoy(void) { return (void *)0; }
