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

void print_promot() { printf("yiyiya$"); }

void print_help() { printf("supports help ps cd pwd command\n"); }
void print_string(char* str) { printf(str); }

void do_shell_cmd(char* cmd) {
  if (strncmp(cmd, "help", 4) == 0) {
    print_help();
  } else if (strncmp(cmd, "cd", 2) == 0) {
    // cd_command(cmd);
  } else if (strncmp(cmd, "pwd", 3) == 0) {
    // pwd_command();
  } else if (strncmp(cmd, "ps", 2) == 0) {
    // ps_command();
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
  if (pid == 0) {  // 子进程
    // reopen( "/dev/log");
    execv(cmd, argv);
    exit(0);
  }
  return 0;
}

int do_exec(char* cmd) {
  char buf[64];
  char* argv[64];
  int i = 0;
  const char* split = " ";
  char* ptr = strtok(cmd, split);
  while (ptr != NULL) {
    argv[i++] = ptr;
    ptr = strtok(NULL, split);
  }
  if (i <= 0 || argv[1] == ' ' || argv[0] == NULL) {
    return 0;
  }
  sprintf(buf, "/bin/%s", argv[0]);
  int ret = access(buf, 0);
  if (ret < 0) {
    sprintf(buf, "/%s", argv[0]);
    ret = access(buf, 0);
  }
  int pid = 0;
  if (ret == 0) {
    pid = run_exec(buf, argv);
  }
  return pid;
}

void shell_loop() {
  char buf[128];
  int count = 0;
  int ret = 0;

  for (;;) {
    int ch = getchar();
    if (ch > 0) {
      if (ch == '\r' || ch == '\n') {
        print_string("\n");
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
      }
    } else {
      sleep(1000);
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

  int series = open("/dev/series", 0);
  if (series < 0) {
    print_string("error open series\n");
  }

  if (dup2(series, 1) < 0) {
    print_string("err in dup2\n");
  }
  if (dup2(series, 0) < 0) {
    print_string("err in dup2\n");
  }
  if (dup2(series, 2) < 0) {
    print_string("err in dup2\n");
  }

  shell_loop();

  return 0;
}