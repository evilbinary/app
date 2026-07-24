#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "cmocka.h"

void test_free_a(void** state) {
  void *p = malloc(1024);
  free(p);
}

void test_free_twice(void **state) {
  void *p = malloc(1024);
  free(p);
  p=NULL;
  free(p);
}

int main(int argc, char *argv[]) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_free_a),
      cmocka_unit_test(test_free_twice)
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}