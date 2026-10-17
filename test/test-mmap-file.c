/* 文件映射（lazy + 缺页补页）测试
 * 内核侧：sysfn.c 的文件 mmap 登记式路径 + page.c 的 file_node 缺页分支。
 * 验证点：
 *   1. mmap 后逐页访问（每页第一次访问都走缺页→valloc+vread）内容正确；
 *   2. 非零页对齐偏移的映射内容正确；
 *   3. 超过文件尾的页读到 0（与旧的全量 kmemset+vread 行为一致）；
 *   4. 写映射（PRIVATE，只写 RAM 不回写文件）可读回；
 *   5. munmap 后半段，前半段仍可读（vfree 容忍已映射页的洞）。
 * 注意：映射期间保持 fd 打开——fatfs 的 close 会关 FIL，映射中途关 fd
 * 再触发缺页会读到空（见 kernel/memory.h 的注释），故本测试 munmap 之
 * 后才 close。 */
#include <fcntl.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "cmocka.h"

#define TFILE "/mmap-test.bin"
#define PAGE 4096
#define NPAGES 8
#define TSIZE (NPAGES*PAGE)

/* 每页填充唯一字节 'A'+i，第 2 页偏移 100 处放标记串 */
static void fill_pattern(unsigned char* buf, int npages) {
  int i;
  for (i = 0; i < npages; i++) {
    memset(buf + i * PAGE, 'A' + i, PAGE);
  }
  strcpy((char*)(buf + 2 * PAGE + 100), "HELLO-MMAP");
}

static int make_test_file(const char* name, const unsigned char* buf, int len) {
  FILE* f = fopen(name, "w");
  if (f == NULL) {
    return -1;
  }
  fwrite(buf, 1, len, f);
  fclose(f);
  return 0;
}

/* 整文件映射：逐页首尾字节都检查（首尾各一次缺页） */
void test_mmap_file_pages(void** state) {
  unsigned char expect[TSIZE];
  unsigned char* p;
  int fd, i;

  (void)state;
  fill_pattern(expect, NPAGES);
  assert_int_equal(make_test_file(TFILE, expect, TSIZE), 0);

  fd = open(TFILE, O_RDONLY);
  assert_int_not_equal(fd, -1);

  p = mmap(NULL, TSIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  assert_int_not_equal(p, MAP_FAILED);

  for (i = 0; i < NPAGES; i++) {
    assert_int_equal(p[i * PAGE], 'A' + i);          /* 页首 */
    assert_int_equal(p[i * PAGE + PAGE - 1], 'A' + i); /* 页尾 */
  }
  assert_string_equal((char*)(p + 2 * PAGE + 100), "HELLO-MMAP");

  assert_int_equal(munmap(p, TSIZE), 0);
  close(fd);
}

/* 非零页对齐偏移：从第 2 页起映射 */
void test_mmap_file_offset(void** state) {
  unsigned char expect[TSIZE];
  unsigned char* p;
  int fd, i;

  (void)state;
  fill_pattern(expect, NPAGES);
  assert_int_equal(make_test_file(TFILE, expect, TSIZE), 0);

  fd = open(TFILE, O_RDONLY);
  assert_int_not_equal(fd, -1);

  p = mmap(NULL, 2 * PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 2 * PAGE);
  assert_int_not_equal(p, MAP_FAILED);
  for (i = 0; i < 2 * PAGE; i++) {
    assert_int_equal(p[i], expect[2 * PAGE + i]);
  }
  assert_int_equal(munmap(p, 2 * PAGE), 0);
  close(fd);
}

/* 小文件大映射：超过文件尾的页读到 0 */
void test_mmap_file_beyond_eof(void** state) {
  unsigned char expect[PAGE];
  unsigned char* p;
  int fd, i;

  (void)state;
  memset(expect, 'Z', PAGE);
  assert_int_equal(make_test_file(TFILE, expect, PAGE), 0);

  fd = open(TFILE, O_RDONLY);
  assert_int_not_equal(fd, -1);

  p = mmap(NULL, 4 * PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  assert_int_not_equal(p, MAP_FAILED);

  assert_int_equal(p[0], 'Z');
  assert_int_equal(p[PAGE - 1], 'Z');
  for (i = 1; i < 4; i++) {
    assert_int_equal(p[i * PAGE], 0);                  /* 尾外页首 */
    assert_int_equal(p[i * PAGE + PAGE - 1], 0);       /* 尾外页尾 */
  }
  assert_int_equal(munmap(p, 4 * PAGE), 0);
  close(fd);
}

/* 写映射：PRIVATE，读回自己写的值（不要求回写文件） */
void test_mmap_file_write(void** state) {
  unsigned char expect[2 * PAGE];
  unsigned char* p;
  int fd;

  (void)state;
  fill_pattern(expect, 2);
  assert_int_equal(make_test_file(TFILE, expect, 2 * PAGE), 0);

  fd = open(TFILE, O_RDONLY);
  assert_int_not_equal(fd, -1);

  p = mmap(NULL, 2 * PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  assert_int_not_equal(p, MAP_FAILED);

  p[0] = 0x5a;
  p[PAGE + 123] = 0xa5;
  assert_int_equal(p[0], 0x5a);
  assert_int_equal(p[PAGE + 123], 0xa5);
  /* 未写过的位置内容不丢 */
  assert_int_equal(p[1], 'A');
  assert_int_equal(p[PAGE + 1], 'B');

  assert_int_equal(munmap(p, 2 * PAGE), 0);
  close(fd);
}

/* munmap 后半段：前半段内容仍正确（vfree 按页 page_v2p，容忍洞） */
void test_mmap_file_partial_unmap(void** state) {
  unsigned char expect[4 * PAGE];
  unsigned char* p;
  int fd, i;

  (void)state;
  fill_pattern(expect, 4);
  assert_int_equal(make_test_file(TFILE, expect, 4 * PAGE), 0);

  fd = open(TFILE, O_RDONLY);
  assert_int_not_equal(fd, -1);

  p = mmap(NULL, 4 * PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  assert_int_not_equal(p, MAP_FAILED);

  assert_int_equal(munmap(p + 2 * PAGE, 2 * PAGE), 0);
  for (i = 0; i < 2 * PAGE; i++) {
    assert_int_equal(p[i], expect[i]);
  }
  /* 再映射一次同区域，内容应仍从文件来 */
  {
    unsigned char* q = mmap(NULL, 2 * PAGE, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE, fd, 2 * PAGE);
    assert_int_not_equal(q, MAP_FAILED);
    assert_int_equal(q[0], 'A' + 2);
    assert_int_equal(q[PAGE], 'A' + 3);
    assert_int_equal(munmap(q, 2 * PAGE), 0);
  }

  assert_int_equal(munmap(p, 2 * PAGE), 0);
  close(fd);
}

int main(int argc, char* argv[]) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_mmap_file_pages),
      cmocka_unit_test(test_mmap_file_offset),
      cmocka_unit_test(test_mmap_file_beyond_eof),
      cmocka_unit_test(test_mmap_file_write),
      cmocka_unit_test(test_mmap_file_partial_unmap),
  };
  (void)argc;
  (void)argv;
  return cmocka_run_group_tests(tests, NULL, NULL);
}
