#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#include "cmocka.h"

void test_malloc_free(void** state) {
  void* p = malloc(412);
  assert_non_null(p);
  free(p);
}

void test_malloc_large(void** state) {
  void* p = malloc(1024 * 1024);
  assert_non_null(p);
  free(p);
}

void test_my_realloc(void** state) {
  char* str;
  str = (char*)malloc(5);
  strcpy(str, "evil");
  printf("str = %s,  addr = %p\n", str, str);
  assert_string_equal(str, "evil");

  str = (char*)realloc(str, 25);
  strcat(str, "binary");
  printf("str = %s,  addr = %p\n", str, str);
  assert_string_equal(str, "evilbinary");
  free(str);
}

void test_my_calloc(void** state) {
  int n = 10;
  int* a = calloc(n, sizeof(int));
  for (int i = 0; i < n; i++) {
    a[i] = i;
  }
  for (int i = 0; i < n; i++) {
    assert_true(a[i] == i);
  }
  free(a);
}


static char* loadfile(FILE* f, int* len) {
  int c, l = 0, p = 0;
  char *d = 0, buf[512];

  for (;;) {
    c = fread(buf, 1, sizeof buf, f);
    if (c <= 0) break;
    l += c;
    d = realloc(d, l);
    assert_non_null(d);
    if (!d) return 0;
    assert_non_null(d + p);
    char *pp=d+p;
    memcpy(d + p, buf, c);
    p += c;
  }
  *len = l;
  return d;
}


void test_my_realloc_multi(void** state) {
  FILE* fp;
  char* name="/mario.gba";
  fp = fopen(name, "r+");
  assert_non_null(fp);
  printf("fd=%d\n", *fp);
  int len=0;
  loadfile(fp,&len);
  printf("len=%d\n", len);
}

int main(int argc, char* argv[]) {
  const struct CMUnitTest tests[] = {
      // cmocka_unit_test(test_malloc_free), cmocka_unit_test(test_my_realloc),
      // cmocka_unit_test(test_my_calloc), cmocka_unit_test(test_malloc_large)
    cmocka_unit_test(test_my_realloc_multi)
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}