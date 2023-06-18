#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

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
    // c=0xff;
    // if(i== 123456){
    //   c=0;
    // }
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
  // char* name = "/01.gb";
  char* name = "/pokemon.gbc";

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

#define MAP_ARRAY_SIZE 9
#define MAP_FLAGS (MAP_ANONYMOUS | MAP_PRIVATE)
#define PAGE_SIZE 4096

static const struct {
  void* addr;
  int ret;
  unsigned int flags;
} gmmap_test[MAP_ARRAY_SIZE] = {
    {(void*)0, 0, MAP_FLAGS},
    {(void*)1, -1, MAP_FLAGS | MAP_FIXED},
    {(void*)(PAGE_SIZE - 1), -1, MAP_FLAGS | MAP_FIXED},
    {(void*)PAGE_SIZE, -1, MAP_FLAGS | MAP_FIXED},
    {(void*)-1, 0, MAP_FLAGS},
    {(void*)(-PAGE_SIZE), -1, MAP_FLAGS | MAP_FIXED},
    {(void*)(-1 - PAGE_SIZE), -1, MAP_FLAGS | MAP_FIXED},
    {(void*)(-1 - PAGE_SIZE - 1), -1, MAP_FLAGS | MAP_FIXED},
    {(void*)(0x1000 * PAGE_SIZE), 0, MAP_FLAGS | MAP_FIXED},
};

void test_mmap() {
  void* p = NULL;
  int i;
  int ret;
  int count = sizeof(gmmap_test) / sizeof(gmmap_test[0]);
  void* array[MAP_ARRAY_SIZE] = {NULL};

  for (i = 0; i < count; i++) {
    p = mmap((void*)gmmap_test[i].addr, PAGE_SIZE,
             PROT_READ | PROT_WRITE | PROT_EXEC, gmmap_test[i].flags, -1, 0);
    ret = (p == MAP_FAILED) ? -1 : 0;
    printf("=>%d %x %x\n", i, gmmap_test[i].ret, ret);
    assert_int_equal(gmmap_test[i].ret, ret);
    array[i] = p;
  }

  for (i = 0; i < count; i++) {
    if (array[i] == MAP_FAILED) {
      continue;
    }
    ret = munmap(array[i], PAGE_SIZE);
    assert_int_equal(ret, 0);
  }
}

#define MAP_TEST_FILE "/dev/stdin"

void test_mmap1() {
  char* p1 = NULL;
  char* p2 = NULL;
  char* p3 = NULL;
  int fd, page_size;
  int ret;

  page_size = getpagesize();
  fd = open(MAP_TEST_FILE, O_RDONLY);
  assert_int_not_equal(fd, -1);

  p1 = (char*)mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  assert_int_not_equal(p1, MAP_FAILED);
  (void)memset(p1, page_size, 0, page_size);

  p2 = (char*)mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  assert_int_not_equal(p2, MAP_FAILED);
  (void)memset(p2, page_size, 0, page_size);

  ret = memcmp(p1, p2, page_size);
  assert_int_equal(ret, 0);

  p1[0] = 1;
  assert_int_equal(p2[0], 0);

  p2[0] = 2;
  assert_int_equal(p1[0], 1);

  p3 = (char*)mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  assert_int_not_equal(p3, MAP_FAILED);
  assert_int_equal(p3[0], 0xf);

  ret = munmap(p1, page_size);
  assert_int_equal(ret, 0);
  ret = munmap(p2, page_size);
  assert_int_equal(ret, 0);
  ret = munmap(p3, page_size);
  assert_int_equal(ret, 0);
  ret = close(fd);
  assert_int_equal(ret, 0);
}

int main(int argc, char* argv[]) {
  const struct CMUnitTest tests[] = {
    // cmocka_unit_test(test_malloc_free),
  //                                    cmocka_unit_test(test_my_realloc),
  //                                    cmocka_unit_test(test_my_calloc),
  //                                    cmocka_unit_test(test_malloc_large),
                                     cmocka_unit_test(test_my_realloc_multi),
                                    //  cmocka_unit_test(test_mremap),
                                    //  cmocka_unit_test(test_realloc_large),
                                    //  cmocka_unit_test(test_brk),
                                    //  cmocka_unit_test(test_mmap),
                                    //  cmocka_unit_test(test_mmap1)

  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}