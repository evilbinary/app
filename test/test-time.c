#include <dirent.h>
#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cmocka.h"

void test_time() {
  struct timespec ts;
  struct timespec start_ts;
  struct timespec now;
  int result = clock_gettime(1, &ts);


  printf("sizeof timespec %d time_t %d long %d\n",sizeof(struct timespec),sizeof(time_t),sizeof(long));

  // 检查返回值
  assert_int_equal(result, 0);

  // 打印时间（秒和纳秒）
  printf("Seconds: %ld, Nanoseconds: %ld\n", ts.tv_sec, ts.tv_nsec);

  // 检查秒和纳秒的合理性
  assert_true(ts.tv_sec > 0);
  // assert_in_range(ts.tv_nsec, 0, 999999999);

  result = clock_gettime(1, &start_ts);
  assert_int_equal(result, 0);
  printf("start_ts Seconds: %ld, Nanoseconds: %ld\n", start_ts.tv_sec,
         start_ts.tv_nsec);

  sleep(5);
  result = clock_gettime(1, &now);
  assert_int_equal(result, 0);

  printf("now Seconds: %ld, Nanoseconds: %ld\n", now.tv_sec, now.tv_nsec);

  uint64_t ticks = (uint64_t)(((int64_t)(now.tv_sec - start_ts.tv_sec) * 1000) +
                              ((now.tv_nsec - start_ts.tv_nsec) / 1000000));

  printf("ticks=%ld\n", ticks);

  assert_true(ticks > 0);
}

int main(int argc, char *argv[]) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_time),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}