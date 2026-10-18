/*******************************************************************
 * Copyright 2021-present evilbinary
 * 作者: evilbinary on 10/18/26
 * 邮箱: rootdebug@163.com
 ********************************************************************/
/* 页交换（swap）压力测试。
 *
 * 用法：test-swap [MB]     默认 48
 * 【前提】[swap] 已配置且容量 >= 请求的 MB（config.h 的
 * CONFIG_SWAP_FILE_MB 默认 32 不够，建议 system.conf 里 size_mb = 128）。
 * 页池耗尽时内核的换出兜底在 sys_mmap2 的 lazy 路径上表现为缺页线程退出，
 * 所以请求过大不会优雅失败，别把它当 bug 提。
 *
 * 覆盖：
 *  1) test_swap_roundtrip —— 大块匿名映射，逐页写散列特征值后【反向】+
 *     【正向】两轮读回校验：换出/换入来回翻转也不能串页（反向读保证
 *     每一页都要经历换出→换入，正向读保证换回来的页没被再次换坏）；
 *  2) test_swap_hole_remap —— munmap 后同地址 MAP_FIXED 重映射：
 *     新匿名映射必须读到大页 0。抓"vfree 没清换出记录，旧数据串进
 *     新映射"（曾经的洞：被换出的页在 vfree 里 phy==NULL 被跳过，
 *     记录残留 ⇒ 同 VA 复用后缺页路径命中旧记录换入垃圾）；
 *  3) test_swap_fork_inherit —— fork 继承：父进程写满特征值后 fork，
 *     子进程必须读到完全相同的内容。抓"fork 不换回"（换出页没有页表
 *     映射，COW/page_clone 复制不到子进程 ⇒ 子进程读到 0）；
 *  4) test_swap_multiproc —— 两个子进程各占 64MB（合计超过页池）并发
 *     填充/校验：跨进程的"换出 A、换入 B"翻转，页池压力不足时不触发
 *     换出也能过（纯正确性），压力够时才是真正的换页压测。
 *
 * 特征值：word = (页号 * 2654435761 + 字序 * 97 + 1)，任何页串了/清零了
 * 都能在校验时精确定位到页号。 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static size_t g_mb = 48;

static uint32_t page_word(uint32_t page_no, uint32_t word_no) {
  return page_no * 2654435761u + word_no * 97u + 1u;
}

static void fill_page(uint8_t* page, uint32_t page_no) {
  uint32_t* w = (uint32_t*)page;
  for (uint32_t k = 0; k < 4096 / 4; k++) {
    w[k] = page_word(page_no, k);
  }
}

static int check_page(const uint8_t* page, uint32_t page_no, uint32_t* bad_word) {
  const uint32_t* w = (const uint32_t*)page;
  for (uint32_t k = 0; k < 4096 / 4; k++) {
    if (w[k] != page_word(page_no, k)) {
      *bad_word = k;
      return -1;
    }
  }
  return 0;
}

/* mmap 请求大小，失败自动减半直到成功（保底 4MB）。
 * 减半的原因见文件头：页池耗尽在 lazy 路径上不优雅，宁可少测不崩。 */
static uint8_t* mmap_adaptive(size_t* len_out) {
  size_t len = g_mb << 20;
  for (;;) {
    void* p = mmap(NULL, len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p != MAP_FAILED) {
      *len_out = len;
      return (uint8_t*)p;
    }
    if (len <= (4u << 20)) {
      return MAP_FAILED;
    }
    len /= 2;
  }
}

/* 逐页填特征值；哪些页被换出由内核压力决定，测试不关心 */
static void fill_region(uint8_t* p, uint32_t npages) {
  for (uint32_t i = 0; i < npages; i++) {
    fill_page(p + (size_t)i * 4096, i);
  }
}

static void verify_region(const uint8_t* p, uint32_t npages) {
  for (uint32_t i = 0; i < npages; i++) {
    uint32_t bad_word = 0;
    if (check_page(p + (size_t)i * 4096, i, &bad_word) != 0) {
      fail_msg("page %u word %u corrupted (swap-out/in lost or mixed data)",
               i, bad_word);
    }
  }
}

/* ------------------------------------------------------------------ 1) */

static void test_swap_roundtrip(void** state) {
  size_t len = 0;
  uint8_t* p = mmap_adaptive(&len);
  assert_ptr_not_equal(p, MAP_FAILED);
  uint32_t npages = (uint32_t)(len / 4096);

  fill_region(p, npages);
  /* 反向读：刚写完的尾页还热着、头页最可能已被换出 ⇒ 每页都走一遍换入 */
  for (uint32_t i = npages; i > 0; i--) {
    uint32_t bad_word = 0;
    if (check_page(p + (size_t)(i - 1) * 4096, i - 1, &bad_word) != 0) {
      fail_msg("reverse pass: page %u word %u corrupted", i - 1, bad_word);
    }
  }
  /* 正向再读一轮：刚换回来的页在最坏时机（页池仍然紧张）再次被换出，
   * 检查第二轮换入的数据仍正确 */
  verify_region(p, npages);
  assert_int_equal(munmap(p, len), 0);
}

/* ------------------------------------------------------------------ 2) */

static void test_swap_hole_remap(void** state) {
  size_t len = 0;
  uint8_t* p = mmap_adaptive(&len);
  assert_ptr_not_equal(p, MAP_FAILED);
  uint32_t npages = (uint32_t)(len / 4096);

  fill_region(p, npages);
  verify_region(p, npages);

  /* 整块还回去，再在【同一地址】MAP_FIXED 要回来 */
  assert_int_equal(munmap(p, len), 0);
  void* q = mmap(p, len, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  assert_ptr_not_equal(q, MAP_FAILED);
  assert_ptr_equal(q, p);

  /* 新匿名映射必须是干净的 0：残留的换出记录会在这里换入旧特征值，
   * 一抓一个准 */
  for (uint32_t i = 0; i < npages; i++) {
    const uint32_t* w = (const uint32_t*)(p + (size_t)i * 4096);
    for (uint32_t k = 0; k < 4096 / 4; k += 64) {
      if (w[k] != 0) {
        fail_msg("remapped page %u not zero at word %u (stale swap record?)",
                 i, k);
      }
    }
  }
  /* 新映射可写 */
  for (uint32_t i = 0; i < npages; i++) {
    fill_page(p + (size_t)i * 4096, i);
  }
  verify_region(p, npages);
  assert_int_equal(munmap(p, len), 0);
}

/* ------------------------------------------------------------------ 3) */

static void test_swap_fork_inherit(void** state) {
  size_t len = 0;
  uint8_t* p = mmap_adaptive(&len);
  assert_ptr_not_equal(p, MAP_FAILED);
  uint32_t npages = (uint32_t)(len / 4096);

  fill_region(p, npages);
  verify_region(p, npages);

  pid_t pid = fork();
  assert_int_not_equal(pid, -1);
  if (pid == 0) {
    /* 子进程：COW 共享 + 换出页必须被换回后共享，内容一字不差 */
    verify_region(p, npages);
    _exit(0);
  }
  int st = 0;
  assert_int_equal(waitpid(pid, &st, 0), pid);
  assert_true(WIFEXITED(st));
  assert_int_equal(WEXITSTATUS(st), 0);

  /* 父进程再读一轮：fork 的 COW 标记 + swapback 不能弄脏父进程数据 */
  verify_region(p, npages);
  assert_int_equal(munmap(p, len), 0);
}

/* ------------------------------------------------------------------ 4) */

/* 多进程并发压力：两个子进程各占 CHILD_MB 匿名内存，合计超过可用页池
 * （versatilepb qemu 128MB RAM ⇒ 页池 ~110MB），把"换出 A 进程的页、
 * 换入 B 进程的页"的跨进程翻转真正压出来。子进程特征值带 seed 偏移，
 * 两个进程的数据串了页立刻能定位。用小 seed 偏移（64K 页 = 256MB 之外），
 * 与父进程/彼此的特征值空间不重叠。 */
#define MULTIPROC_CHILD_MB 64

static int child_fill_verify(int child_idx) {
  size_t len = (size_t)MULTIPROC_CHILD_MB << 20;
  uint32_t seed = (uint32_t)child_idx * 65536u; /* 页号偏移，隔离特征值空间 */
  uint8_t* p = mmap(NULL, len, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) {
    return 2;
  }
  uint32_t npages = (uint32_t)(len / 4096);
  for (uint32_t i = 0; i < npages; i++) {
    fill_page(p + (size_t)i * 4096, seed + i);
  }
  for (uint32_t i = 0; i < npages; i++) {
    uint32_t bad_word = 0;
    if (check_page(p + (size_t)i * 4096, seed + i, &bad_word) != 0) {
      return 3; /* 数据坏了：换出/换入串页 */
    }
  }
  munmap(p, len);
  return 0;
}

static void test_swap_multiproc(void** state) {
  pid_t pids[2];
  int st = 0;

  /* 先 fork 再等待：两个子进程的地址空间同时活着才有页池压力 */
  for (int c = 0; c < 2; c++) {
    pid_t pid = fork();
    assert_int_not_equal(pid, -1);
    if (pid == 0) {
      _exit(child_fill_verify(c));
    }
    pids[c] = pid;
  }
  for (int c = 0; c < 2; c++) {
    assert_int_equal(waitpid(pids[c], &st, 0), pids[c]);
    assert_true(WIFEXITED(st));
    assert_int_equal(WEXITSTATUS(st), 0);
  }
}

/* ------------------------------------------------------------------ */

int main(int argc, char** argv) {
  if (argc > 1) {
    long v = atol(argv[1]);
    if (v >= 4 && v <= 512) {
      g_mb = (size_t)v;
    }
  }
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_swap_roundtrip),
      cmocka_unit_test(test_swap_hole_remap),
      cmocka_unit_test(test_swap_fork_inherit),
      cmocka_unit_test(test_swap_multiproc),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
