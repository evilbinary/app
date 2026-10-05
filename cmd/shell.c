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
#include <sys/wait.h>
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

static int shell_split_args(char* cmd, char** out, int max); /* 前向声明 */

void cd_command(char* cmd) {
  char* argv[8];
  char* ptr;
  /* 引号感知切分 ✓（cd "my dir" 也能用）*/
  shell_split_args(cmd, argv, 8);
  ptr = argv[1];
  if (ptr == NULL) {
    return;
  }
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
  if (pid == 0) {
    execv(cmd, argv);
    exit(1);
  }
  if (pid > 0) {
    int status = 0;
    waitpid(pid, &status, 0);
  }
  return pid;
}

/* YiYiYa·修复：按【引号感知】切分命令行 —— 原实现 strtok(" ") 只按空格切 ✗，
 * console-os 传来的带引号路径（consoleShellQuote： "roms/nes/The Legend.nes"）
 * 会被切成多个 token：argv[1] =  "roms/nes/The  ⇒ 路径被空格截断、引号也去不掉
 * ⇒ infones 打不开 ROM（open nes faild）✗。
 * 现在：遇 ' 或 " 就吞到匹配的引号为止（引号内的空格不切 ✓，引号被剥掉 ✓）。 */
static int shell_split_args(char* cmd, char** out, int max) {
  int n = 0;
  char* p = cmd;
  while (*p && n < max - 1) {
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) break;
    if (*p == '"' || *p == '\'') {
      char q = *p++;
      out[n++] = p;
      while (*p && *p != q) p++;
      if (*p == q) *p++ = '\0';
    } else {
      out[n++] = p;
      while (*p && *p != ' ' && *p != '\t') p++;
      if (*p) *p++ = '\0';
    }
  }
  out[n] = NULL;
  return n;
}

int do_exec(char* cmd) {
  char cmd_buf[128];
  char* cmd_argv[64];
  int i = 0;
  int ret;
  memset(cmd_buf, 0, sizeof(cmd_buf));
  memset(cmd_argv, 0, sizeof(cmd_argv));
  /* 引号感知切分：带空格的路径（"roms/nes/The Legend.nes"）也正确 ✓ */
  i = shell_split_args(cmd, cmd_argv, 64);
  if (i <= 0 || cmd_argv[0] == NULL) {
    return 0;
  }
  /* 带斜杠的按路径打开（./yes、/mnt/sda2/bin/yes）。
   * 没有斜杠才去 /bin、/ 里找，避免 /bin/./yes。 */
  if (strchr(cmd_argv[0], '/') != NULL) {
    strncpy(cmd_buf, cmd_argv[0], sizeof(cmd_buf) - 1);
    cmd_buf[sizeof(cmd_buf) - 1] = '\0';
    ret = access(cmd_buf, 0);
  } else {
    sprintf(cmd_buf, "/bin/%s", cmd_argv[0]);
    ret = access(cmd_buf, 0);
    if (ret < 0) {
      sprintf(cmd_buf, "/%s", cmd_argv[0]);
      ret = access(cmd_buf, 0);
    }
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

  /* `sh -c "cmd"`：供 system()/popen 等使用，执行完即退出 */
  if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
    return do_exec(argv[2]);
  }

  const char* home = getenv("HOME");
  if (home == NULL || home[0] == 0) {
    home = "/";
  }
  chdir(home);

  print_promot();
  shell_loop();

  return 0;
}