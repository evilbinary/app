/*******************************************************************
 * Copyright 2021-2080 evilbinary
 * 作者: evilbinary on 01/01/20
 * 邮箱: rootdebug@163.com
 ********************************************************************/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

enum {
  SYS_EXIT = 1,
  SYS_FORK = 2,
  SYS_READ = 3,
  SYS_WRITE = 4,
  SYS_OPEN = 5,
  SYS_CLOSE = 6,
  SYS_UNLINK = 10,
  SYS_EXEC = 11,
  SYS_CHDIR = 12,
  SYS_SEEK = 19,
  SYS_GETPID = 20,
  SYS_ALARM = 27,
  SYS_ACESS = 33,
  SYS_KILL = 37,
  SYS_RENAME = 38,
  SYS_MKDIR = 39,
  SYS_PIPE = 42,
  SYS_DUP = 41,
  SYS_BRK = 45,
  SYS_SBRK = 46,  //?
  SYS_IOCTL = 54,
  SYS_UMASK = 60,
  SYS_DUP2 = 63,
  SYS_GETPPID = 64,
  SYS_READLINK = 85,
  SYS_READDIR = 89,
  SYS_MUNMAP = 91,
  SYS_STAT = 106,
  SYS_FSTAT = 108,
  SYS_SYSINFO = 116,
  SYS_CLONE = 120,
  SYS_MPROTECT = 125,
  SYS_FCHDIR = 133,
  SYS_LLSEEK = 140,
  SYS_GETDENTS = 141,
  SYS_READV = 145,
  SYS_WRITEV = 146,
  SYS_YIELD = 158,
  SYS_NANOSLEEP = 162,
  SYS_MREMAP = 163,
  SYS_RT_SIGACTION = 174,
  SYS_RT_SIGPROCMASK = 175,
  SYS_GETCWD = 183,
  SYS_MMAP2 = 192,
  SYS_MADVISE = 220,
  SYS_FCNT64 = 221,
  SYS_GETDENTS64 = 217,  // diff from x86
  SYS_CLOCK_NANOSLEEP = 230,
  SYS_FUTEX = 240,
  SYS_SET_THREAD_AREA = 243,
  SYS_EXIT_GROUP = 248,  // diff from x86
  SYS_SET_TID_ADDRESS = 256,
  SYS_STATX = 397,
  SYS_CLOCK_GETTIME64 = 403,
  SYS_PRINT = 500,
  SYS_PRINT_AT = 501,
  SYS_DEV_READ = 502,
  SYS_DEV_WRITE = 503,
  SYS_DEV_IOCTL = 504,
  SYS_TEST = 505,
  SYS_MAP = 506,
  SYS_UMAP = 507,
  SYS_VALLOC = 508,
  SYS_VFREE = 509,
  SYS_VHEAP = 510,
  SYS_DUMPS = 511,
  SYS_SELF = 512,
  SYS_MEMINFO = 513,
  SYS_THREAD_SELF = 514,
  SYS_THREAD_CREATE = 515,
  SYS_THREAD_DUMP = 516,
  SYS_THREAD_ADDR = 517,
};

void print_promot() {
  printf("yiyiya$");
  fflush(stdout);
}

void print_help() { printf("supports help ps cd pwd command\n"); }
void print_string(char* str) {
  printf(str);
  fflush(stdout);
}

void cd_command(char* cmd) {
  char buf[64];
  char* argv[64];
  int i = 0;
  const char* split = " ";
  char* ptr = strtok(cmd, split);
  ptr = strtok(NULL, split);
  print_string(ptr);
  chdir(ptr);
}

void pwd_command() {
  char buf[128];
  memset(buf, 0, 128);
  getcwd(buf, 128);
  print_string(buf);
  print_string("\n");
}
void ps_command() { syscall(SYS_DUMPS); }

void mem_info_command() { syscall(SYS_MEMINFO); }

void do_shell_cmd(char* cmd) {
  if (strncmp(cmd, "help", 4) == 0) {
    print_help();
  } else if (strncmp(cmd, "cd", 2) == 0) {
    cd_command(cmd);
  } else if (strncmp(cmd, "pwd", 3) == 0) {
    pwd_command();
  } else if (strncmp(cmd, "ps", 2) == 0) {
    ps_command();
  } else if (strncmp(cmd, "mem", 3) == 0) {
    // mem_info_command();
  } else {
    int ret = do_exec(cmd);
    if (ret < 0) {
      print_string(cmd);
      print_string(" command not support\n");
    }
  }
}

int run_exec(char* cmd, char** argv) {
  int pid = fork();
  int p = syscall(SYS_GETPID);
  if (pid == 0) {  // 子进程
    // reopen( "/dev/log");
    execv(cmd, argv);
    exit(0);
  }
  return p;
}

int do_exec(char* cmd) {
  char cmd_buf[64];
  char* cmd_argv[64];
  int i = 0;
  const char* split = " ";
  char* ptr = strtok(cmd, split);
  while (ptr != NULL) {
    cmd_argv[i++] = ptr;
    ptr = strtok(NULL, split);
  }
  if (i <= 0 || cmd_argv[1] == ' ' || cmd_argv[0] == NULL) {
    return 0;
  }
  sprintf(cmd_buf, "/bin/%s", cmd_argv[0]);
  int ret = access(cmd_buf, 0);
  if (ret < 0) {
    sprintf(cmd_buf, "/%s", cmd_argv[0]);
    ret = access(cmd_buf, 0);
  }
  int pid = 0;
  if (ret == 0) {
    pid = run_exec(cmd_buf, cmd_argv);
  }
  return pid;
}

void shell_loop() {
  char buf[128];
  int count = 0;
  int ret = 0;
  int ch;
  memset(buf, 0, 128);
  for (;;) {
    ch = getchar();
    ret = ch;
    if (ret > 0) {
      if (ch == '\r' || ch == '\n') {
        printf("\n");
        do_shell_cmd(buf);
        count = 0;
        memset(buf, 0, 128);
        print_promot();
      } else if (ch == 127) {
        if (count > 0) {
          print_string("\n");
          buf[--count] = 0;
          print_promot();
          print_string(buf);
        }
      } else {
        buf[count++] = ch;
        putchar(ch);
        fflush(stdout);
      }
    } else {
      sleep(100);
    }
  }
}

int main(int argc, char* argv[]) {
  setenv("PATH", "/bin:/app/", 1);

  const char* home = getenv("HOME");
  if (home == NULL || home[0] == 0) {
    home = "/";
  }
  chdir(home);

  print_promot();
  shell_loop();

  return 0;
}