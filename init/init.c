/*******************************************************************
 * Copyright 2021-present evilbinary
 * 作者: evilbinary on 01/01/20
 * 邮箱: rootdebug@163.com
 ********************************************************************/
#include "init.h"

#define VERSION "1.4"

/* 命令行缓冲：原来是 64，而且读字符时没有上限检查 —— 一行超过 63 字符就
 * 越界写 init 自己的栈：buf 旁边的 count / ret / env / series 全被命令行内容
 * 盖掉。真机现象就是长命令（yllm chat --model ... --vocab ... --prompt ...）
 * 参数被截断、execve 收到 ASCII 垃圾 envp（实测 0x6d6f7270 = "prom"）。
 * init 跑在用户栈上（1MB），1KB 静态缓冲随便放。 */
#define INIT_LINE_MAX 1024


const char* logo =
    " _  _  _ _  _  _      \n"
    "| || || | || || |     \n"
    "| \\| |/ | \\| |/ __  __\n"
    " \\_   _/ \\_   _/  \\/ /\n"
    "   | |     | |( ()  < \n"
    "   |_|     |_| \\__/\\_\\\n\n"
    "2021 - present Copyright by evilbinary \n";

const char* build_str = "version " VERSION " " __DATE__ " " __TIME__
                        "\n"
                        "https://github.com/evilbinary/YiYiYa\n";

const char* welcome = "\nWelcome to YiYiYa Os ^_^! \n\n";

void print_string(char* str) { syscall1(SYS_PRINT, str); }

void print_cpu_info(void) {
  char buf[128];
  
#if defined(ARM64) || defined(__aarch64__)
  kstrcpy(buf, "CPU: ARM64 (ARMv8-A)\n");
#elif defined(ARMV7_A)
  kstrcpy(buf, "CPU: ARMv7-A\n");
#elif defined(ARMV7)
  kstrcpy(buf, "CPU: ARMv7\n");
#elif defined(ARMV5)
  kstrcpy(buf, "CPU: ARMv5\n");
#elif defined(X86)
  kstrcpy(buf, "CPU: x86\n");
#else
  kstrcpy(buf, "CPU: Unknown\n");
#endif
  
  print_string(buf);
}

void print_logo() {
  print_string(logo);
  print_string(build_str);
  print_cpu_info();
  print_string(welcome);
}

void print_promot() { print_string("yiyiya$"); }

void print_help() { print_string("supports help ps cd pwd trace command\n"); }

void ps_command() { syscall0(SYS_DUMPS); }

void mem_info_command() { syscall0(SYS_MEMINFO); }

void pre_launch();
void build_env(char** env);

extern int module_ready;

// STM32F4XX不使用fork，使用线程
#ifdef STM32F4XX
// 使用cmd方式
#else
#define USE_FORK 1
#endif

// void hello_thread(void) {
//   print_string("hello\n");
//   char env_buf[512];
//   char** env = env_buf;
//   build_env(env);
//   char* cmd = "/ls";
//   syscall3(SYS_EXEC, cmd, NULL, env);
//   syscall1(SYS_EXIT, 0);
// }

void reopen(char* name) {
  int series = syscall2(SYS_OPEN, name, 0);
  if (series <= 0) {
    print_string("error open \n");
  }

  if (syscall2(SYS_DUP2, series, 1) < 0) {
    print_string("err in dup2\n");
  }
  if (syscall2(SYS_DUP2, series, 0) < 0) {
    print_string("err in dup2\n");
  }
  if (syscall2(SYS_DUP2, series, 2) < 0) {
    print_string("err in dup2\n");
  }
}

extern int cmd_exec(const char* name);

int run_exec(char* cmd, char** argv, char** env) {
#ifdef USE_FORK
  char path[INIT_LINE_MAX];
  char* exec_argv[64];
  int argc = 0;

  if (cmd != NULL) {
    kstrncpy(path, cmd, sizeof(path) - 1);
  } else if (argv != NULL && argv[0] != NULL) {
    ksnprintf(path, sizeof(path), "/bin/%s", argv[0]);
  } else {
    return -1;
  }
  path[sizeof(path) - 1] = '\0';

  /* Pass full argv through to exec (was wrongly truncated to argv[0] only). */
  if (argv != NULL) {
    while (argc < 63 && argv[argc] != NULL) {
      exec_argv[argc] = argv[argc];
      argc++;
    }
  }
  if (argc == 0) {
    exec_argv[0] = path;
    argc = 1;
  }
  exec_argv[argc] = NULL;

  int pid = syscall0(SYS_FORK);
  if (pid == 0) {
    int ret = syscall3(SYS_EXEC, path, exec_argv, env);
    syscall1(SYS_EXIT, ret < 0 ? 1 : 0);
  }
  if (pid > 0) {
    // int status = 0;
    // syscall4(SYS_WAIT4, pid, &status, 0, 0);
  }
  return pid;
#else
  // 先尝试从命令表查找
  return cmd_exec(cmd);
#endif
}

int do_exec(char* cmd, int count, char** env) {
  char buf[INIT_LINE_MAX];
  cmd[count] = 0;
  char* argv[64];
  int i = 0;
  const char* split = " ";
  char* ptr = kstrtok(cmd, split);
  memset(argv, 0, sizeof(argv));
  while (ptr != NULL && i < 63) {
    argv[i++] = ptr;
    ptr = kstrtok(NULL, split);
  }
  argv[i] = NULL;
  if (i <= 0 || argv[0] == NULL) {
    return 0;
  }
  int ret =0;
#ifdef USE_FORK
  /* ./yes 这种路径按当前目录打开，不要拼成 /bin/./yes。 */
  int has_slash = 0;
  for (ptr = argv[0]; *ptr != 0; ptr++) {
    if (*ptr == '/') {
      has_slash = 1;
      break;
    }
  }
  if (has_slash) {
    ksnprintf(buf, sizeof(buf), "%s", argv[0]);
    ret = syscall2(SYS_ACESS, buf, 0);
  } else {
    ksnprintf(buf, sizeof(buf), "/bin/%s", argv[0]);
    ret = syscall2(SYS_ACESS, buf, 0);
    if (ret < 0) {
      ksnprintf(buf, sizeof(buf), "/%s", argv[0]);
      ret = syscall2(SYS_ACESS, buf, 0);
      if (ret < 0) {
        ksnprintf(buf, sizeof(buf), "./%s", argv[0]);
        ret = syscall2(SYS_ACESS, buf, 0);
      }
    }
  }
#else
ksnprintf(buf, sizeof(buf), "%s", argv[0]);
#endif
  int pid = 0;
  if (ret == 0) {
    pid = run_exec(buf, argv, env);
  }
  return pid;
}

void cd_command(char* cmd, int count) {
  char buf[64];
  cmd[count] = 0;
  char* argv[64];
  int i = 0;
  const char* split = " ";
  char* ptr = kstrtok(cmd, split);
  ptr = kstrtok(NULL, split);
  print_string(ptr);
  syscall1(SYS_CHDIR, ptr);
}
void trace_command(char* cmd, int count) {
  int buf[64];
  cmd[count] = 0;
  char* argv[64];
  int i = 0;
  const char* split = " ";
  char* ptr = kstrtok(cmd, split);
  ptr = kstrtok(NULL, split);
  if (ptr != NULL) {
    buf[0] = katoi(&ptr);
  }

  ptr = kstrtok(NULL, split);
  if (ptr != NULL) {
    buf[1] = katoi(&ptr);
  } else {
    buf[1] = 300;
  }

  ptr = kstrtok(NULL, split);
  if (ptr != NULL) {
    buf[2] = katoi(&ptr);
  }

  int trace = syscall2(SYS_OPEN, "/dev/trace", 0);
  if (trace < 0) {
    print_string("error open trace\n");
  }

  syscall3(SYS_IOCTL, trace, 1, buf);
}

void pwd_command() {
  char buf[128];
  kmemset(buf, 0, 128);
  syscall2(SYS_GETCWD, buf, 128);
  print_string(buf);
  print_string("\n");
}

void do_shell_cmd(char* cmd, int count, char** env) {
  if (count == 0) return;
  if (kstrncmp(cmd, "help", count) == 0) {
    print_help();
  } else if (kstrncmp(cmd, "cd", 2) == 0) {
    cd_command(cmd, count);
  } else if (kstrncmp(cmd, "pwd", 3) == 0) {
    pwd_command();
  } else if (kstrncmp(cmd, "ps", 2) == 0) {
    ps_command();
  } else if (kstrncmp(cmd, "mem", 3) == 0) {
    mem_info_command();
  } else if (kstrncmp(cmd, "trace", 5) == 0) {
    trace_command(cmd, count);
  } else {
    int ret = do_exec(cmd, count, env);
    if (ret < 0) {
      print_string(cmd);
      print_string(" command not support\n");
    }
  }
  kmemset(cmd, 0, count);
}

void sleep() {
  struct timespec tv;
  tv.tv_nsec = 500 * 1000 * 1000;
  tv.tv_sec = 0;
  syscall4(SYS_CLOCK_NANOSLEEP, 0, 0, &tv, &tv);
}

#ifndef WNOHANG
#define WNOHANG 1
#endif

static void reap_zombies(void) {
  int status = 0;
  while (syscall4(SYS_WAIT4, -1, &status, WNOHANG, 0) > 0) {
  }
}

void do_init_thread(void) {
  char buf[INIT_LINE_MAX];
  int count = 0;
  int ret = 0;

  /* Must not SYS_PRINT or schedule_sleep until posix is registered. */
  while (module_ready <= 0) {
  }

  print_string("init\n");
  print_logo();
  print_promot();

  int series = syscall2(SYS_OPEN, "/dev/series", 0);
  if (series <= 0) {
    print_string("error open series\n");
  }

  if (syscall2(SYS_DUP2, series, 1) < 0) {
    print_string("err in dup2\n");
  }
  if (syscall2(SYS_DUP2, series, 0) < 0) {
    print_string("err in dup2\n");
  }
  if (syscall2(SYS_DUP2, series, 2) < 0) {
    print_string("err in dup2\n");
  }
  syscall1(SYS_CHDIR, "/");

  char** env = NULL;
  pre_launch();

  /* 一次 read 尽可能多取：以前每次只 read 1 字节，粘贴一条长命令时
   * UART 的 RX FIFO 会在用户态一个个 syscall+回显的过程中溢出丢字符。 */
  char in[64];
  int in_len = 0;
  int in_pos = 0;
  for (;;) {
    int ch = 0;
    reap_zombies();
    if (in_pos >= in_len) {
      in_pos = 0;
      in_len = syscall3(SYS_READ, 0, in, sizeof(in));
      if (in_len < 0) {
        in_len = 0;
      }
    }
    if (in_pos < in_len) {
      ch = (unsigned char)in[in_pos++];
      if (ch == '\r' || ch == '\n') {
        print_string("\n");
        do_shell_cmd(buf, count, env);
        count = 0;
        memset(buf, 0, sizeof(buf));
        print_promot();
      } else if (ch == 127) {
        if (count > 0) {
          print_string("\n");
          buf[--count] = 0;
          print_promot();
          print_string(buf);
        }
      } else {
        /* 边界：以前是 buf[count++] 裸写，超过 63 字符直接踩栈 */
        if (count >= (int)sizeof(buf) - 1) {
          int bell = '\a';
          syscall3(SYS_WRITE, 1, &bell, 1);
          continue;
        }
        buf[count++] = ch;
        syscall3(SYS_WRITE, 1, &ch, 1);
      }
    } else {
      sleep();
    }
  }
}

void try_run(char* cmd, char** argv, char** env) {
  char buf[INIT_LINE_MAX];
  ksnprintf(buf, sizeof(buf), "/bin/%s", argv[0]);
  int ret = syscall2(SYS_ACESS, buf, 0);
  if (ret < 0) {
    ksnprintf(buf, sizeof(buf), "/%s", argv[0]);
    ret = syscall2(SYS_ACESS, buf, 0);
  }
  if (ret >= 0) {
    run_exec(buf, argv, env);
  }
}

void load_config() {
  char* config_argv[] = {"config", NULL};
  try_run("config", config_argv, NULL);


  // char* gmenu_argv[] = {"gmenu", NULL};
  // try_run("gmenu", gmenu_argv, NULL);


  // char* shell_argv[] = {"shell", NULL};
  // try_run("shell", shell_argv, NULL);


  // char* etk_argv[] = {"etk", NULL};
  // try_run("etk", etk_argv, NULL);

  // char* config_argv[] = {"lvgl", NULL};
  // try_run("lvgl", config_argv, NULL);
  // char* config_argv[] = {"infones","/mario.nes", NULL};
  // try_run("infones", config_argv, NULL);
}

void pre_launch() {
  // must init global for armv7-a
  char* scm_argv[] = {"/scheme", "-b", "/scheme.boot", "--verbose", NULL};
  char* lua_argv[] = {"/lua", "/hello.lua", NULL};
  char* mgba_argv[] = {"mgba", "/mario.gba", NULL};
  char* cat_argv[] = {"/cat", "hello.lua", NULL};
  char* showimg_argv[] = {"/showimage", "/pngtest.png", NULL};
  char* gnuboy_argv[] = {"/gnuboy", "./pokemon.gbc", NULL};
  char* nes_argv[] = {"infones", "/mario.nes", NULL};
  char* shell_argv[] = {"shell", NULL, NULL};

#ifdef X86
  // int fd = syscall2(SYS_OPEN, "/dev/stdin", 0);
  // syscall3(SYS_EXEC,"/ls",NULL,NULL);
  // syscall3(SYS_EXEC,"/gui",NULL,NULL);
  // syscall3(SYS_EXEC,"/test-file",NULL,NULL);
  // syscall3(SYS_EXEC,"/test-mem",NULL,NULL);
  // syscall3(SYS_EXEC,"/test-uncompress",NULL,NULL);
  // syscall3(SYS_EXEC,"/test-string",NULL,NULL);
  // syscall3(SYS_EXEC,"/test-stdlib",NULL,NULL);
  // syscall3(SYS_EXEC,"/test-stdio",NULL,NULL);

  // syscall3(SYS_EXEC, "/luat", NULL);

  // syscall3(SYS_EXEC, "/etk", NULL);
  // syscall3(SYS_EXEC,"/test-rs",NULL,NULL);
  // syscall3(SYS_EXEC, "/lua", lua_argv,NULL);
  // syscall3(SYS_EXEC,"/launcher",NULL,NULL);
  // syscall3(SYS_EXEC,"/track",NULL,NULL);
  // syscall3(SYS_EXEC,"/test",NULL,NULL);
  // syscall3(SYS_EXEC,"/microui",NULL,NULL);
  // syscall3(SYS_EXEC,"/lvgl",NULL,NULL);
  // kprintf("fd=>%d\n",fd);

  // syscall3(SYS_EXEC, "/infones", nes_argv,NULL);
  // syscall3(SYS_EXEC, "/mgba", mgba_argv,NULL);
  // syscall3(SYS_EXEC, "/scheme", scm_argv,NULL);
  // syscall3(SYS_EXEC, "/sdl2", NULL,NULL);
  // syscall3(SYS_EXEC, "/showimage", showimg_argv,NULL);
  // syscall3(SYS_EXEC, "/gnuboy", gnuboy_argv,NULL);

  // for (;;)
  //   ;
#elif defined(ARMV7_A)
  //  try_run("/bin/shell", shell_argv, env_p);
  // try_run("infones", nes_argv, NULL);
  load_config();

#elif defined(ARMV8_A)
  //  try_run("/bin/shell", shell_argv, env_p);
  // try_run("infones", nes_argv, NULL);
  load_config();
#elif defined(ARMV5)

  load_config();

#elif defined(ARMV7)
  // test_lcd();
#else defined(ARM)
  // syscall3(SYS_EXEC,"/hello-rs",NULL,NULL);
  // syscall3(SYS_EXEC,"/test-rs",NULL,NULL);
  // syscall3(SYS_EXEC,"/ls",NULL,NULL);
  // syscall3(SYS_EXEC, "/test", NULL);
  // syscall3(SYS_EXEC,"/hello",NULL,NULL);
  // syscall3(SYS_EXEC, "/lvgl", NULL);
  // syscall3(SYS_EXEC, "/launcher", NULL);

  // syscall3(SYS_EXEC,"/track",NULL,NULL);
  // syscall3(SYS_EXEC,"/gui",NULL,NULL);
  // syscall3(SYS_EXEC,"/etk",NULL,NULL);
  //  syscall3(SYS_EXEC,"/test",NULL,NULL);
  //  syscall3(SYS_EXEC,"/microui",NULL,NULL);

  // syscall3(SYS_EXEC, "/lua", lua_argv,NULL);

  // syscall3(SYS_EXEC,"/test-musl",NULL,NULL);
  // syscall3(SYS_EXEC, "/scheme", scm_argv,NULL);
  // syscall3(SYS_EXEC, "/sdl2", NULL,NULL);
  // syscall3(SYS_EXEC, "/mgba", mgba_argv,NULL);
  // syscall3(SYS_EXEC, "/player", mgba_argv,NULL);
  // syscall3(SYS_EXEC, "/cat", cat_argv,NULL);
  // syscall3(SYS_EXEC, "/infones", nes_argv,NULL);
  // syscall3(SYS_EXEC,"/test-file",NULL,NULL);
  // syscall3(SYS_EXEC, "/gnuboy", gnuboy_argv,NULL);
  // syscall3(SYS_EXEC, "/showimage", showimg_argv,NULL);

// test_cpu_speed();
//  for(;;);
#endif
}
