/*******************************************************************
 * panel — 自研极简任务栏（替代 lxpanel）。
 *
 * 结构：屏幕顶部一条 override-redirect 窗口（openbox 不会再给它加边框），
 * 自己做曝光重绘：
 *
 *   ┌───────────────────────────────────────────────┐
 *   │ Menu   xclock   xeyes   Terminal        HH:MM │
 *   └───────────────────────────────────────────────┘
 *
 * 点 "Menu" 弹一个 override-redirect 小窗（抓指针，点外面即关闭），
 * 点条目 fork+exec 起程序。时钟每 30 秒刷一次。
 *
 * 字体用 X 核心字体 "fixed"（Xorg 内置，不依赖 /usr/share/fonts）。
 * 只依赖 app/x11/x11.c 的协议层。
 *
 * 用法：panel            （DISPLAY 无关，直连 /tmp/.X11-unix/X0）
 ********************************************************************/
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "x11.h"

#define BAR_H 26
#define BAR_BG X11_RGB(0x2e, 0x34, 0x40)
#define BAR_FG X11_RGB(0xd8, 0xde, 0xe9)
#define SEL_BG X11_RGB(0x4c, 0x56, 0x6a)
#define POP_W 170
#define ITEM_H 24
#define POP_PAD 4
#define CLOCK_TICK 30

struct menu_item {
  const char* label;
  const char* exec;
};

static struct menu_item items[] = {
    {"Terminal", "/usr/bin/lxterminal"},
    {"xclock", "/usr/bin/xclock"},
    {"xeyes", "/usr/bin/xeyes"},
};
#define N_ITEMS ((int)(sizeof(items) / sizeof(items[0])))

static x11_t c;
static uint32_t bar, bar_gc, font, popup, popup_gc;
static int popup_open;
static int pop_w, pop_h;

static void spawn(const char* path) {
  pid_t p = fork();
  if (p == 0) {
    execl(path, path, (char*)NULL);
    fprintf(stderr, "panel: exec %s failed\n", path);
    _exit(127);
  }
}

static int text_len(const char* s) { return (int)strlen(s); }

static void draw_text(uint32_t win, int x, int y, const char* s) {
  x11_image_text8(&c, win, bar_gc, x, y, s, text_len(s));
}

static void draw_clock(void) {
  char buf[32];
  time_t now = time(NULL);
  struct tm* tm = gmtime(&now);
  int right;
  if (tm == NULL) {
    return;
  }
  snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
  right = c.w - 10 - text_len(buf) * 6; /* fixed 字体宽 6px */
  x11_set_gc(&c, bar_gc, 0x0C /*FG|BG*/, BAR_FG, BAR_BG);
  draw_text(bar, right, 17, buf);
}

static void draw_bar(void) {
  x11_set_gc(&c, bar_gc, 0x0C, BAR_BG, BAR_BG);
  x11_fill_rect(&c, bar, bar_gc, 0, 0, c.w, BAR_H);
  x11_set_gc(&c, bar_gc, 0x0C, BAR_FG, BAR_BG);
  draw_text(bar, 10, 17, "Menu");
  draw_text(bar, 70, 17, "|");
  draw_clock();
}

static void draw_popup(void) {
  int i;
  x11_set_gc(&c, popup_gc, 0x0C, BAR_BG, BAR_BG);
  x11_fill_rect(&c, popup, popup_gc, 0, 0, pop_w, pop_h);
  for (i = 0; i < N_ITEMS; i++) {
    int y = POP_PAD + i * ITEM_H;
    x11_set_gc(&c, popup_gc, 0x0C, BAR_FG, BAR_BG);
    x11_image_text8(&c, popup, popup_gc, 10, y + 16, items[i].label,
                    text_len(items[i].label));
  }
}

static void popup_close(void) {
  if (!popup_open) {
    return;
  }
  x11_ungrab_pointer(&c);
  x11_unmap_window(&c, popup);
  popup_open = 0;
}

static void popup_open_at(int x) {
  int i;
  pop_w = POP_W;
  pop_h = POP_PAD * 2 + N_ITEMS * ITEM_H;
  if (x + pop_w > c.w) {
    x = c.w - pop_w;
  }
  if (x < 0) {
    x = 0;
  }
  if (popup == 0) {
    popup = x11_create_window(&c, x, BAR_H, pop_w, pop_h, BAR_BG, 1,
                              X11_EV_EXPOSURE | X11_EV_BUTTON_PRESS);
    popup_gc = x11_id(&c);
    x11_create_gc(&c, popup_gc, popup);
  }
  /* 位置变化只能靠 ConfigureWindow，v1 简化：一次定位，之后复用 */
  (void)i;
  x11_map_window(&c, popup);
  x11_grab_pointer(&c, popup, X11_EV_BUTTON_PRESS);
  popup_open = 1;
  draw_popup();
  x11_clear(&c, popup);
}

static void on_button(const uint8_t* ev) {
  uint32_t win = x11_ev_u32(ev, 12);
  int x = (int16_t)(ev[24] | (ev[25] << 8));
  int y = (int16_t)(ev[26] | (ev[27] << 8));
  if (win == bar) {
    if (x < 60 && !popup_open) {
      popup_open_at(4);
    }
    return;
  }
  if (win == popup && popup_open) {
    int idx = (y - POP_PAD) / ITEM_H;
    int local_y = (y - POP_PAD) % ITEM_H;
    int in_x = (x >= 0 && x < pop_w);
    popup_close();
    if (in_x && local_y >= 0 && idx >= 0 && idx < N_ITEMS) {
      spawn(items[idx].exec);
    }
  }
}

int main(int argc, char* argv[]) {
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  uint8_t ev[32];
  time_t last = 0;
  int refreshed;

  (void)argc;
  (void)argv;
  if (x11_connect(&c, NULL) < 0) {
    return 1;
  }
  bar = x11_create_window(&c, 0, 0, c.w, BAR_H, BAR_BG, 1,
                          X11_EV_EXPOSURE | X11_EV_BUTTON_PRESS);
  bar_gc = x11_id(&c);
  font = x11_id(&c);
  if (bar == 0 || x11_create_gc(&c, bar_gc, bar) < 0 ||
      x11_open_font(&c, font, "fixed") < 0) {
    fprintf(stderr, "panel: 初始化失败\n");
    return 1;
  }
  x11_set_gc_font(&c, bar_gc, font);
  x11_map_window(&c, bar);
  printf("panel: %dx%d bar\n", c.w, BAR_H);

  for (;;) {
    struct pollfd pfd;
    pfd.fd = c.fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, 1000) > 0) {
      while (x11_next_event(&c, ev) == 1) {
        if (ev[0] == X11_EXPOSE) {
          uint32_t win = x11_ev_u32(ev, 4);
          if (win == bar) {
            draw_bar();
          } else if (win == popup) {
            draw_popup();
          }
        } else if (ev[0] == X11_BUTTON_PRESS) {
          on_button(ev);
        }
      }
    }
    refreshed = 0;
    {
      time_t now = time(NULL);
      if (now != last) {
        last = now;
        if (now % CLOCK_TICK == 0 || last == 0) {
          refreshed = 1;
        }
      }
    }
    /* 每 30 秒重画一次时钟（不做局部刷新，整条重绘最简单） */
    if (refreshed) {
      draw_clock();
    }
    /* 收掉退出的子进程，避免僵尸 */
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
  }
  return 0;
}
