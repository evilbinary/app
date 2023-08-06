#include <dirent.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <fcntl.h>
#include "cmocka.h"
#include "stdio.h"
#include "stdlib.h"
#include "sys/ioctl.h"
#include "types.h"
#include "unistd.h"

#define IOC_PTY_MAGIC 'p'

#define IOC_SLAVE _IOW(IOC_PTY_MAGIC, 1, int)
#define IOC_READ_OFFSET _IOW(IOC_PTY_MAGIC, 3, int)
#define IOC_WRITE_OFFSET _IOW(IOC_PTY_MAGIC, 4, int)

char* buf = "hello test elf\n";

void test_pty() {
  u32 fd_ptm, fd_pts;
  fd_ptm = open("/dev/ptm", O_RDWR);
  if (fd_ptm < 0) {
    printf("error get ptm \n");
    assert_true(false);
  }
  u32 pts = ioctl(fd_ptm, IOC_SLAVE);
  char buf[20];
  sprintf(buf, "/dev/pts/%d", pts);
  fd_pts = open(buf, O_RDONLY);
  if (fd_pts < 0) {
    printf("error get pts \n");
    assert_true(false);
  }
  printf("ptm %d pts %d\n", fd_ptm, fd_pts);

  char* buf2 = "abcdef";
  int len = strlen(buf2);
  write(fd_ptm, buf2, len);
  memset(buf2, 0, len);
  read(fd_pts, buf2, len);
  printf("read from ptm: %s\n", buf2);
  assert_string_equal(buf2, "abcdef");
}

void test_dup() {
  FILE* fp = fopen("/testdup.txt", "w");
  printf("open fp %x\n", fp);
  assert_non_null(fp);
  int fno = fileno(fp);
  fno = STDOUT_FILENO;

  int fd = dup(fno);
  printf("dup fd %d\n", fd);
  assert_true(fp > 0);

  FILE* nfp = fdopen(fd, "w");
  fprintf(nfp, "test dup\n");

  assert_non_null(nfp);

  close(fd);
}

void test_dup2() {
  int fd;
  fd = open("/testdup2.txt", O_CREAT);
  assert_true(fd > 0);

  if (fd < 0) {
    printf("open error\n");
    exit(-1);
  }
  if (dup2(fd, STDOUT_FILENO) < 0) {
    printf("err in dup2\n");
    assert_false(true);
  }
  printf("test str\n");
  close(fd);
}

void test_exec() {
  char* argv[] = {
      "ls",
      "-al",
      "/etc/passwd",
  };
  // execv("/bin/ls", argv);
  execv("/hello", argv);
}

void test_malloc_free() {
  for (int i = 0; i < 100; i++) {
    void* p = malloc(1024 * 2);
    if (p == NULL) {
      printf("malloc error\n");
      assert_non_null(p);
    } else {
      free(p);
    }
  }
}

void test_syscall() {
  // printf("haha\n");
#ifdef LIBYC
  syscall0(12);
#endif
}

void test_scanf() {
  int i = 0;
  scanf("%d", &i);
  printf("i=%d\n", i);

  assert_true(i == 0);
}

void test_getcwd() {
  char buf[256] = {0};
  char* test = getcwd(buf, 20);
  printf("pwd=>%s\n", buf);
  int ret = chdir("/haha");
  test = getcwd(buf, 20);
  printf("pwd=>%s\n", buf);
  if (ret == 0) {
    assert_string_equal(buf, "/haha");
  } else {
    assert_string_equal(buf, "/");
  }
}

int main(int argc, char* argv[]) {
  printf(buf);

  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_getcwd),      cmocka_unit_test(test_scanf),
      cmocka_unit_test(test_malloc_free), cmocka_unit_test(test_syscall),
      cmocka_unit_test(test_dup),         cmocka_unit_test(test_dup2),
      cmocka_unit_test(test_pty),         cmocka_unit_test(test_exec),//fix me
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}