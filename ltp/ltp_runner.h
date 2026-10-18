/* LTP 统一 runner：每个测试 fork 子进程执行（崩溃隔离），父进程汇总结果。
 * 新 API 测试经 wrapper 暴露 struct tst_test*，调 tst_run_tcases；
 * 旧 API 测试经宏改名后的 main 直接调用。 */
#ifndef LTP_RUNNER_H__
#define LTP_RUNNER_H__

struct tst_test;

typedef struct {
  const char *name;                 /* 测试名（如 fork01） */
  struct tst_test *(*get)(void);    /* 新 API：返回测试结构；旧 API 为 0 */
  int (*old_run)(int, char **);     /* 旧 API：原 main；新 API 为 0 */
} ltp_entry_t;

extern const ltp_entry_t ltp_tests[];
extern const int ltp_test_count;

#endif
