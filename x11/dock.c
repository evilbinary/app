/*******************************************************************
 * dock — 自研底部 dock（应用启动器 + 运行中窗口按钮）。
 *
 * 屏幕底部一条 override-redirect 窗口，画两段：
 *   左边  固定启动器：终端 / 时钟 / xeyes（彩色方块 + 字母）
 *   右边  运行中的窗口：读根窗口 _NET_CLIENT_LIST，每个窗口一个按钮，
 *         标题取该窗口的 WM_NAME；点一下发 _NET_ACTIVE_WINDOW 给根窗口，
 *         由 wm 负责 raise + SetInputFocus
 *
 * 每 800ms 轮询一次 _NET_CLIENT_LIST，变了就重画（窗口开关能立刻反映）。
 * 图像资源：没有图标文件，方块+字母用 X 核心字体 "fixed" 画，零依赖。
 *
 * 用法：dock           （直连 /tmp/.X11-unix/X0）
 ********************************************************************/
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "x11.h"

#define DOCK_H 56
#define ICON 40
#define PAD 8
#define STEP (ICON + PAD)
#define MAX_RUN 12

#define DOCK_BG X11_RGB(0x24, 0x2a, 0x33)
#define DOCK_BG2 X11_RGB(0x2e, 0x34, 0x40)
#define DOCK_FG X11_RGB(0xec, 0xef, 0xf4)
#define RUN_ON X11_RGB(0x5e, 0x81, 0xac)
#define RUN_OFF X11_RGB(0x43, 0x4c, 0x5e)

struct launcher {
  const char* letter;
  const char* exec;
  uint32_t color;
};

static struct launcher launch[] = {
    {"T", "/usr/bin/lxterminal", X11_RGB(0xa3, 0xbe, 0x8c)},
    {"C", "/usr/bin/xclock", X11_RGB(0x88, 0xc0, 0xd0)},
    {"E", "/usr/bin/xeyes", X11_RGB(0xeb, 0xcb, 0x8b)},
};
#define N_LAUNCH ((int)(sizeof(launch) / sizeof(launch[0])))

static x11_t c;
static uint32_t dock, gc, font;
static uint32_t atom_client_list, atom_active;
static uint32_t runs[MAX_RUN];
static int nruns;

static void spawn(const char* path) {
  pid_t p = fork();
  if (p == 0) {
    execl(path, path, (char*)NULL);
    _exit(127);
  }
}

static void text(uint32_t win, int x, int y, const char* s) {
  x11_image_text8(&c, win, gc, x, y, s, (int)strlen(s));
}

/* 正方形按钮 + 居中字母 */
static void button(int idx, uint32_t color, const char* label, int running) {
  int x = PAD + idx * STEP;
  int y = (DOCK_H - ICON) / 2;
  x11_set_gc(&c, gc, 0x0C, color, color);
  x11_fill_rect(&c, dock, gc, x, y, ICON, ICON);
  if (running) { /* 运行中的窗口加一条底部高亮 */
    x11_set_gc(&c, gc, 0x0C, RUN_ON, RUN_ON);
    x11_fill_rect(&c, dock, gc, x, DOCK_H - 5, ICON, 3);
  }
  x11_set_gc(&c, gc, 0x0C, DOCK_FG, color);
  x11_image_text8(&c, dock, gc, x + ICON / 2 - 3, y + ICON / 2 + 6, label, 1);
}

static void draw(void) {
  int i;
  x11_set_gc(&c, gc, 0x0C, DOCK_BG, DOCK_BG);
  x11_fill_rect(&c, dock, gc, 0, 0, 2000, DOCK_H);
  x11_set_gc(&c, gc, 0x0C, DOCK_BG2, DOCK_BG2);
  x11_fill_rect(&c, dock, gc, 0, 0, 2000, 1);

  for (i = 0; i < N_LAUNCH; i++) {
    button(i, launch[i].color, launch[i].letter, 0);
  }
  for (i = 0; i < nruns; i++) {
    button(N_LAUNCH + i, RUN_OFF, "W", 1);
  }
  x11_clear(&c, dock);
}

/* 读 _NET_CLIENT_LIST，变化则返回 1 */
static int refresh_runs(void) {
  uint32_t list[MAX_RUN];
  int n = x11_get_property32(&c, c.root, atom_client_list, list, MAX_RUN);
  int i, changed = (n != nruns);
  if (n < 0) {
    n = 0;
  }
  if (!changed) {
    for (i = 0; i < n; i++) {
      if (list[i] != runs[i]) {
        changed = 1;
        break;
      }
    }
  }
  if (changed) {
    nruns = n;
    for (i = 0; i < n; i++) {
      runs[i] = list[i];
    }
  }
  return changed;
}

static void activate(uint32_t win) {
  uint8_t ev[32];
  memset(ev, 0, sizeof(ev));
  ev[0] = X11_CLIENT_MESSAGE;
  ev[1] = 32;
  /* 发送给根窗口，data[0] = 目标窗口（与 wm.c 的约定一致） */
  ev[4] = (uint8_t)(c.root & 0xff);
  ev[5] = (uint8_t)((c.root >> 8) & 0xff);
  ev[6] = (uint8_t)((c.root >> 16) & 0xff);
  ev[7] = (uint8_t)((c.root >> 24) & 0xff);
  ev[8] = (uint8_t)(atom_active & 0xff);
  ev[9] = (uint8_t)((atom_active >> 8) & 0xff);
  ev[10] = (uint8_t)((atom_active >> 16) & 0xff);
  ev[11] = (uint8_t)((atom_active >> 24) & 0xff);
  ev[12] = (uint8_t)(win & 0xff);
  ev[13] = (uint8_t)((win >> 8) & 0xff);
  ev[14] = (uint8_t)((win >> 16) & 0xff);
  ev[15] = (uint8_t)((win >> 24) & 0xff);
  x11_send_event(&c, c.root, X11_EV_STRUCTURE, ev); /* SubstructureNotify */
}

static void on_button(const uint8_t* ev) {
  int x = (int16_t)(ev[24] | (ev[25] << 8));
  int idx;
  if (x < PAD) {
    return;
  }
  idx = (x - PAD) / STEP;
  if ((x - PAD) % STEP > ICON) { /* 点在间隙里 */
    return;
  }
  if (idx < N_LAUNCH) {
    spawn(launch[idx].exec);
    return;
  }
  idx -= N_LAUNCH;
  if (idx >= 0 && idx < nruns) {
    activate(runs[idx]);
  }
}

int main(int argc, char* argv[]) {
  uint8_t ev[32];
  int tick = 0;
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  (void)argc;
  (void)argv;

  printf("dock: connecting\n");
  if (x11_connect(&c, NULL) < 0) {
    return 1;
  }
  printf("dock: connected %dx%d depth %d\n", c.w, c.h, (int)c.root_depth);
  gc = x11_id(&c);
  font = x11_id(&c);
  if (x11_open_font(&c, font, "fixed") < 0) {
    fprintf(stderr, "dock: 打开 fixed 字体失败\n");
    return 1;
  }
  x11_create_gc(&c, gc, c.root);
  x11_set_gc_font(&c, gc, font);
  printf("dock: gc/font ok\n");
  atom_client_list = x11_intern_atom(&c, X11_NET_CLIENT_LIST);
  atom_active = x11_intern_atom(&c, X11_NET_ACTIVE_WINDOW);

  dock = x11_create_window(&c, 0, c.h - DOCK_H, c.w, DOCK_H, DOCK_BG, 1,
                           X11_EV_EXPOSURE | X11_EV_BUTTON_PRESS);
  if (dock == 0) {
    fprintf(stderr, "dock: 建窗口失败\n");
    return 1;
  }
  printf("dock: win=%u atoms=%u/%u\n", dock, atom_client_list, atom_active);
  x11_map_window(&c, dock);
  x11_raise(&c, dock);
  printf("dock: mapped runs=%d\n", refresh_runs());
  draw();
  printf("dock: drawn\n");
  printf("dock: %dx%d 底部 dock\n", c.w, DOCK_H);

  for (;;) {
    struct pollfd pfd;
    pfd.fd = c.fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, 800) > 0) {
      while (x11_next_event(&c, ev) == 1) {
        if (ev[0] == X11_EXPOSE) {
          if (x11_ev_u32(ev, 4) == dock) {
            draw();
          }
        } else if (ev[0] == X11_BUTTON_PRESS) {
          if (x11_ev_u32(ev, 12) == dock) {
            on_button(ev);
          }
        }
      }
    }
    if (++tick >= 1) { /* 每次超时（0.8s）查一次窗口列表 */
      tick = 0;
      if (refresh_runs()) {
        draw();
      }
    }
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
  }
  return 0;
}
