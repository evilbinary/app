/*******************************************************************
 * session — 自研极简桌面会话（替代 lxsession）。
 *
 * 职责就三件：
 *   1. 设好 DISPLAY 环境（YiYiYa 的 shell 不支持 `VAR=val cmd` 前缀，
 *      由这里 setenv 后 exec，子进程自然拿到）
 *   2. 按顺序拉起：wm（自研窗口管理器）→ wall（背景）→ panel（任务栏）
 *   3. 守护：wm 退出（或本进程收到 SIGTERM）就把子进程收掉再退出，
 *      避免留下孤儿窗口管理器/任务栏
 *
 * 用法：session [wallpaper.bmp]
 *   默认背景 /root/wallpaper.bmp，存在才设；不存在就只起 WM + panel。
 ********************************************************************/
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define DEFAULT_WALL "/root/wallpaper.bmp"

static pid_t kids[8];
static int nkids;

static void kill_kids(void) {
  int i;
  for (i = 0; i < nkids; i++) {
    if (kids[i] > 0) {
      kill(kids[i], SIGTERM);
    }
  }
}

static void on_signal(int sig) {
  (void)sig;
  kill_kids();
  _exit(0);
}

static pid_t spawn(const char* path, const char* arg) {
  pid_t p = fork();
  if (p == 0) {
    if (arg != NULL) {
      execl(path, path, arg, (char*)NULL);
    } else {
      execl(path, path, (char*)NULL);
    }
    fprintf(stderr, "session: exec %s 失败\n", path);
    _exit(127);
  }
  if (p > 0 && nkids < (int)(sizeof(kids) / sizeof(kids[0]))) {
    kids[nkids++] = p;
  }
  return p;
}

int main(int argc, char* argv[]) {
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  const char* wall = (argc > 1) ? argv[1] : DEFAULT_WALL;
  pid_t wm;
  int status;
  pid_t who;

  setenv("DISPLAY", ":0", 1);
  signal(SIGTERM, on_signal);
  signal(SIGINT, on_signal);

  wm = spawn("/bin/wm", NULL);
  if (wm <= 0) {
    fprintf(stderr, "session: 无法启动 wm\n");
    return 1;
  }
  /* 给窗口管理器一点时间接管根窗口，否则后起的面板可能被当成普通窗口 */
  usleep(400 * 1000);

  if (access(wall, R_OK) == 0) {
    spawn("/bin/wall", wall);
    usleep(300 * 1000);
  } else {
    fprintf(stderr, "session: 没有 %s，跳过背景\n", wall);
  }
  spawn("/bin/panel", NULL);
  spawn("/bin/dock", NULL);
  printf("session: wm/wall/panel/dock 已启动\n");

  for (;;) {
    who = waitpid(-1, &status, 0);
    if (who < 0) {
      break;
    }
    if (who == wm) {
      printf("session: wm 退出，收尾\n");
      kill_kids();
      break;
    }
  }
  return 0;
}
