#include <dirent.h>
#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "cmocka.h"

int g_int = 1;  //数据段的全局变量

/**
[PARENT] wait child exit
[CHILD ] child change local global malloc value to 0
[CHILD ] child exit
[PARENT] child have exit
[PARENT] g_int = 1
[PARENT] local_int = 1
[PARENT] malloc_int = 1 
*/
int test_fork() {
  printf("hello,fork\n");
  assert_int_equal(g_int, 1);

  int local_int = 1;                      //栈上的局部变量
  int* malloc_int = malloc(sizeof(int));  //通过malloc动态分配在堆上的变量
  *malloc_int = 1;
  pid_t pid = fork();
  if (pid == 0) /*子进程*/
  {
    local_int = 0;
    g_int = 0;
    *malloc_int = 0;
    fprintf(stderr, "[CHILD ] child change local global malloc value to 0\n");

    assert_int_equal(g_int, 1);
    assert_int_equal(local_int, 1);
    assert_int_equal(malloc_int, 1);

    free(malloc_int);
    sleep(10);
    fprintf(stderr, "[CHILD ] child exit\n");
    exit(0);
  } else if (pid < 0) {
    printf("fork failed (%s)", strerror(errno));
    return 1;
  }
  assert_int_equal(g_int, 1);
  assert_int_equal(local_int, 1);
  assert_int_equal(malloc_int, 1);

  fprintf(stderr, "[PARENT] wait child exit\n");
  waitpid(pid, NULL, 0);
  fprintf(stderr, "[PARENT] child have exit\n");
  printf("[PARENT] g_int = %d\n", g_int);
  printf("[PARENT] local_int = %d\n", local_int);
  printf("[PARENT] malloc_int = %d\n", local_int);
  free(malloc_int);
  return 0;
}

int main(int argc, char* argv[]) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_fork),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}