#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#include "cmocka.h"

void test_malloc_free(void** state) {
  void* p = malloc(1024);
  memset(p, 0xf, 512);
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
  int i = 0;
  int c, l = 0, p = 0;
  char *d = 0, buf[512];
  for (;;) {
    c = fread(buf, 1, sizeof buf, f);
    if (l % 10000 == 0) printf("ret=%d len=%x %d addr=%x\n", c, l, l, d);
    if (c <= 0) break;
    l += c;
    d = realloc(d, l);
    assert_non_null(d);
    if (!d) return 0;
    assert_non_null(d + p);
    char* pp = d + p;
    memcpy(d + p, buf, c);
    p += c;

    if (i >= 32 && i <= 36) {
      printf("==>%d %x\n", i, l);
      for (int j = 0; j < c; j++) {
        printf("%02x ", buf[j] & 0xff);
      }
      printf("\n");
    }
    i++;
  }
  *len = l;
  return d;
}

void test_my_realloc_multi(void** state) {
  FILE* fp;
  // char* name = "/mario.gba";
  char* name = "/01.gb";

  fp = fopen(name, "r+");
  assert_non_null(fp);
  printf("fd=%d\n", *fp);
  int len = 0;
  loadfile(fp, &len);

  printf("len=%d\n", len);
}

void test_mremap(void** state) {
  void *s, *x;
  x = malloc(8192);

  x = (unsigned int)x + 0x1000;
  x = (unsigned int)x & 0xfffff000;
  s = (void*)mremap(x, 4000, 8, 0);

  char* errstr = strerror(errno);
  // expect mremap: Success
  assert_string_equal(errstr, "No error information");

  printf("old 0x%x new 0x%x\n", x, s);

  assert_ptr_equal(x, s);
}

void test_realloc_large() {
  void* p = malloc(1024 * 1024);
  assert_non_null(p);
  free(p);
  int l = 512;
  void* d = 0;
  for (int i = 0; i < 512; i++) {
    d = realloc(d, l);
    printf("i=>%d addr=%x len=%d\n", i, d, l);
    assert_non_null(d);
  }
}

void test_brk() {
  void* b = sbrk(0);
  int* p = (int*)b;
  /* Move it 2 ints forward */
  brk(p + 2);

  /* Use the ints. */
  *p = 1;
  *(p + 1) = 2;
  assert_true(*p == 1);
  assert_true(*(p + 1) == 2);

  /* Deallocate back. */
  brk(b);

  return 0;
}

int main(int argc, char* argv[]) {
  const struct CMUnitTest tests[] = {cmocka_unit_test(test_malloc_free),
                                     cmocka_unit_test(test_my_realloc),
                                     cmocka_unit_test(test_my_calloc),
                                     cmocka_unit_test(test_malloc_large),
                                     cmocka_unit_test(test_my_realloc_multi),
                                     cmocka_unit_test(test_mremap),
                                     cmocka_unit_test(test_realloc_large),
                                     cmocka_unit_test(test_brk)

  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}