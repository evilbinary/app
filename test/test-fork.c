#include <dirent.h>
#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include "cmocka.h"
#include "fcntl.h"
#include "stdio.h"
#include "stdlib.h"
#include "sys/ioctl.h"
#include "types.h"
#include "unistd.h"

#define IOC_PTY_MAGIC 'p'

#define IOC_SLAVE _IOW(IOC_PTY_MAGIC, 1, int)
#define IOC_READ_OFFSET _IOW(IOC_PTY_MAGIC, 3, int)
#define IOC_WRITE_OFFSET _IOW(IOC_PTY_MAGIC, 4, int)

int g_int = 1;  // 数据段的全局变量

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

  int local_int = 1;                      // 栈上的局部变量
  int* malloc_int = malloc(sizeof(int));  // 通过malloc动态分配在堆上的变量
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

void test_fork2() {
  printf("fork\n");
  u32 i = 0;
  pid_t fpid = fork();
  printf("fork end %d\n", fpid);
  if (fpid == 0) {
    printf("child %d %d %d\n", getppid(), getpid(), fpid);
  } else {
    printf("parent %d %d %d\n", getppid(), getpid(), fpid);
  }
  for (int i = 0; i < 10; i++) {
    printf("ppid:%d pid:%d count:%d\n", getppid(), getpid(), i++);
    // getpid();
    // syscall0(505);
  }
  printf("\n");
}

void test_read_write() {
  printf("fork\n");
  u32 fd_ptm, fd_pts;
  fd_ptm = open("/dev/ptm", O_RDONLY);
  if (fd_ptm < 0) {
    printf("error get ptm \n");
  }
  assert_true(fd_ptm>0);

  u32 i = 0;
  pid_t fpid = fork();
  printf("fork end %d\n", fpid);
  if (fpid == 0) {
    printf("child %d %d %d\n", getppid(), getpid(), fpid);
    u32 pts = ioctl(fd_ptm, IOC_SLAVE);
    char buf[20];
    sprintf(buf, "/dev/pts/%d", pts);
    fd_pts = open(buf, O_RDONLY);
    if (fd_pts < 0) {
      printf("error get pts \n");
    }
    int j = 0;
    int pid = getpid();
    for (;;) {
      char buf2[64];
      int len = 1;
      memset(buf2, 0, len);
      read(fd_pts, buf2, len);
      printf("child pid:%d j:%d read from ptm: %s\n", pid, j++, buf2);
    }
  } else {
    printf("parent %d %d %d\n", getppid(), getpid(), fpid);
    int j = 0;
    int pid = getpid();
    for (;;) {
      char* buf2 = "abcdef";
      int len = strlen(buf2);
      if (j < 2 || j > 1000000) {
        printf("parent pid:%d j:%d write ptm: %s\n", pid, j, buf2);
        write(fd_ptm, buf2, len);
      }
      j++;
    }
  }
  for (int i = 0; i < 1000000; i++) {
    printf("pid:%d count:%d\n", getpid(), i++);
    // getpid();
  }
}

void test_dup_pty() {
  printf("test dup pty\n");
  u32 fd_ptm, fd_pts;
  fd_ptm = open("/dev/ptm", O_RDONLY);
  if (fd_ptm < 0) {
    printf("error get ptm \n");
  }
  assert_true(fd_ptm > 0);
  printf("test dup pty start fork\n");
  u32 i = 0;
  pid_t fpid = fork();
  printf("fork end %d\n", fpid);
  if (fpid == 0) {
    printf("child ppid:%d pid:%d fork ret:%d\n", getppid(), getpid(), fpid);
    u32 pts = ioctl(fd_ptm, IOC_SLAVE);
    char buf[20];
    sprintf(buf, "/dev/pts/%d", pts);
    fd_pts = open(buf, O_RDONLY);
    if (fd_pts < 0) {
      printf("error get pts \n");
    }
    printf(" salve pts:%d\n", fd_pts);
    int j = 0;
    int pid = getpid();
    for (;;) {
      char buf2[64];
      int len = 1;
      memset(buf2, 0, len);
      int ret = read(fd_pts, buf2, len);
      printf("pid:%d %d read from ptm: %s ret:%d\n", pid, j++, buf2, ret);
    }
  } else {
    printf("parent ppid:%d pid:%d fork ret:%d\n", getppid(), getpid(), fpid);
    int j = 0;
    int pid = getpid();
    for (;;) {
      char* buf2 = "abcdef";
      int len = strlen(buf2);
      if (j < 3 || j > 10000000) {
        printf("pid:%d %d write ptm: %s ", pid, j, buf2);
        u32 ret = write(fd_ptm, buf2, len);
        printf("   ret=%d\n");
      }
      j++;
    }
  }
  for (int i = 0; i < 10; i++) {
    printf("pid:%d count:%d\n", getpid(), i++);
    // getpid();
  }
}

void test_pipe() {
  pid_t pid;
  char buf[1024];
  int fd[2];
  char* p = "test for pipe\n";

  if (pipe(fd) == -1) printf("pipe error\n");
  printf(" fd0 %d fd1 %d\n", fd[0], fd[1]);
  pid = fork();
  if (pid < 0) {
    printf("fork err pid %d\n", getpid());
  } else if (pid == 0) {  // child
    printf("child pid %d\n", getpid());
    close(fd[1]);
    printf("start read\n");
    int len = read(fd[0], buf, strlen(p));
    printf("child read====>%s ret:%d\n", buf, len);

    memset(buf, 0, 1024);
    len = read(fd[0], buf, strlen(p));
    printf("child read2====>%s ret:%d\n", buf, len);

    // write(STDOUT_FILENO, buf, len);
    close(fd[0]);
  } else {  // parent
    printf("parent pid %d\n", getpid());
    close(fd[0]);
    for (int i = 0; i < 1000; i++)
      ;
    write(fd[1], p, strlen(p));
    for (int i = 0; i < 1000; i++)
      ;
    write(fd[1], "ABCDEF", strlen(p));
    close(fd[1]);
  }
  for (int i = 0; i < 2000; i++) {
    printf("%d=[%d]", getpid(), i);
  }
}

void test_pc() {
  // printf("fork\n");
  // syscall0(12);
  u32 i = 0;
  pid_t fpid = fork();
  printf("t:%d\n", fpid);
  for (;;) {
    printf("t:%d i=>%d\n", fpid, i++);
  }
}

int main(int argc, char* argv[]) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_fork),
      cmocka_unit_test(test_fork2),
      cmocka_unit_test(test_pipe), //have free bug?
      cmocka_unit_test(test_dup_pty),
      cmocka_unit_test(test_read_write),

  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}