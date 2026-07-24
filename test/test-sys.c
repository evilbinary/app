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

void test_wait() {
  pid_t pid;
  int status, i;
  if (fork() == 0) {
    printf("This is the child process. pid =%d\n", getpid());
    exit(5);
  } else {
    sleep(1);
    printf("This is the parent process, wait for child...\n");
    pid = wait(&status);
    i = WEXITSTATUS(status);
    printf("child's pid =%d . exit status=%d\n", pid, i);
  }
}

void test_wait_child() {
  pid_t pid;
  pid = fork();
  if (pid < 0) {
    perror("fork failed");
    exit(1);
  }
  if (pid == 0) {  // 子进程
    int i;
    int id=getpid();
    for (i = 3; i > 0; i--) {
      printf("This is the child id:%d\n",id);
      sleep(1);
    }
    exit(3);
  } else {  // 父进程
    int stat_val;
    waitpid(pid, &stat_val, 0); /*阻塞等待子进程*/
    if (WIFEXITED(stat_val)) {
      printf("Child exited with code %d\n", WEXITSTATUS(stat_val));
    } else if (WIFSIGNALED(stat_val)) {
      printf("Child terminated abnormally, signal %d\n", WTERMSIG(stat_val));
    }
  }
}

int main(int argc, char *argv[]) {
  const struct CMUnitTest tests[] = {
      // cmocka_unit_test(test_wait),
      cmocka_unit_test(test_wait_child),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}