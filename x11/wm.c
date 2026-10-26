/*******************************************************************
 * wm — 自研极简窗口管理器（替代 openbox）。
 *
 * 只做"能把窗口摆出来并拖动/关闭"的最小实现（reparenting WM）：
 *
 *   1. 抢根窗口 SubstructureRedirect，接管所有顶层窗口
 *   2. MapRequest → 建一个带标题栏的 frame，ReparentWindow 把客户端装进去
 *      再把 frame 映射出来（客户端自己不用改代码）
 *   3. 标题栏：左边显示 WM_NAME，右边一个关闭方块；点标题栏拖动窗口
 *   4. ConfigureRequest 按客户端请求改 frame 尺寸；Unmap/Destroy 收 frame
 *   5. 启动时把已经存在的顶层窗口（先起客户端后起 WM 的情况）也接管
 *
 * 刻意不做的：EWMH（_NET_CLIENT_LIST 等）、图标化/最大化、焦点跟随鼠标、
 * 多桌面、窗口动画。面板(panel) 与背景(wall) 是 override-redirect 窗口，
 * 不会被这里接管。
 *
 * 用法：wm          （直连 /tmp/.X11-unix/X0）
 ********************************************************************/
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "x11.h"

#define TITLE_H 20
#define BORDER 2
#define MAX_CLIENTS 32

/* 事件类型 */
#define EV_UNMAP_NOTIFY 18
#define EV_DESTROY_NOTIFY 17
#define EV_MAP_REQUEST 20
#define EV_CONFIGURE_REQUEST 23
#define EV_MOTION_NOTIFY X11_MOTION_NOTIFY

#define TITLE_BG X11_RGB(0x3b, 0x42, 0x52)
#define TITLE_FG X11_RGB(0xec, 0xef, 0xf4)
#define CLOSE_BG X11_RGB(0xbf, 0x61, 0x6a)

struct client {
  uint32_t win;
  uint32_t frame;
  int w, h;
};

static x11_t c;
static struct client cls[MAX_CLIENTS];
static int ncls;
static uint32_t gc, font;
static uint32_t atom_protocols, atom_delete, atom_name;
static uint32_t atom_client_list, atom_active;

static int drag_on;
static uint32_t drag_frame;
static int drag_dx, drag_dy;

static int find_win(uint32_t win) {
  int i;
  for (i = 0; i < ncls; i++) {
    if (cls[i].win == win) {
      return i;
    }
  }
  return -1;
}

static int find_frame(uint32_t frame) {
  int i;
  for (i = 0; i < ncls; i++) {
    if (cls[i].frame == frame) {
      return i;
    }
  }
  return -1;
}

static void draw_frame(int idx) {
  char title[64];
  int fw = 0, fh = 0;
  int i;
  uint32_t win = cls[idx].win;
  uint32_t frame = cls[idx].frame;

  x11_get_geometry(&c, frame, NULL, NULL, &fw, &fh);
  if (fw <= 0) {
    return;
  }
  x11_set_gc(&c, gc, 0x0C /*FG|BG*/, TITLE_BG, TITLE_BG);
  x11_fill_rect(&c, frame, gc, 0, 0, fw, TITLE_H);
  x11_set_gc(&c, gc, 0x0C, CLOSE_BG, CLOSE_BG);
  x11_fill_rect(&c, frame, gc, fw - TITLE_H, 2, TITLE_H - 4, TITLE_H - 4);

  title[0] = '\0';
  if (x11_get_property_str(&c, win, atom_name, title, sizeof(title)) <= 0 ||
      title[0] == '\0') {
    strcpy(title, "(no name)");
  }
  /* 标题栏文字，留出右侧关闭按钮的位置。
   * 【必须同时按缓冲区长度截断】原来只按 "frame 宽度能放几个字" 算上界：
   * frame 宽度一旦是垃圾值（服务器回了个 Error 被当成 GetGeometry 回复、
   * 或客户端设了很大的初始尺寸），循环就会越过 title[64] 一路写下去，
   * 把 wm 自己的栈踩烂 —— 之后发出的请求内容全是随机的，服务器那边
   * 会按错位的数据解析。 */
  {
    int lim = (fw - TITLE_H - 16) / 6;
    if (lim < 0) {
      lim = 0;
    }
    if (lim > (int)sizeof(title) - 1) {
      lim = (int)sizeof(title) - 1;
    }
    i = (int)strlen(title);
    if (i > lim) {
      i = lim;
    }
    title[i] = '\0';
  }
  x11_set_gc(&c, gc, 0x0C, TITLE_FG, TITLE_BG);
  x11_image_text8(&c, frame, gc, 6, TITLE_H - 5, title, i);
  x11_set_gc(&c, gc, 0x0C, TITLE_FG, CLOSE_BG);
  x11_image_text8(&c, frame, gc, fw - TITLE_H + 5, TITLE_H - 5, "x", 1);
}

/* 把当前客户端窗口列表发布到根窗口 _NET_CLIENT_LIST，dock/panel 据此画任务按钮 */
static void update_client_list(void) {
  uint32_t list[MAX_CLIENTS];
  int i;
  if (ncls <= 0) {
    x11_delete_property(&c, c.root, atom_client_list);
    return;
  }
  for (i = 0; i < ncls; i++) {
    list[i] = cls[i].win;
  }
  x11_set_property32(&c, c.root, atom_client_list, X11_ATOM_WINDOW, list, ncls);
}

static void activate(uint32_t win) {
  int idx = find_win(win);
  if (idx < 0) {
    return;
  }
  x11_raise(&c, cls[idx].frame);
  x11_set_focus(&c, cls[idx].win);
}

static void unmanage(int idx, int destroy_frame) {
  if (idx < 0 || idx >= ncls) {
    return;
  }
  if (destroy_frame && cls[idx].frame != 0) {
    x11_destroy_window(&c, cls[idx].frame);
  }
  cls[idx] = cls[--ncls];
  update_client_list();
}

static void manage(uint32_t win) {
  int x = 0, y = 0, w = 0, h = 0;
  int map_state = 0, override = 0;
  uint32_t frame;
  int idx;

  if (ncls >= MAX_CLIENTS || find_win(win) >= 0) {
    x11_map_window(&c, win);
    return;
  }
  if (x11_get_attrs(&c, win, &map_state, &override, NULL) < 0) {
    return;
  }
  if (override) { /* 菜单/面板这类不该被接管 */
    x11_map_window(&c, win);
    return;
  }
  if (x11_get_geometry(&c, win, &x, &y, &w, &h) < 0 || w <= 0 || h <= 0) {
    x11_map_window(&c, win);
    return;
  }
  frame = x11_create_window(&c, x - BORDER, y - TITLE_H, w + 2 * BORDER,
                           h + TITLE_H + BORDER, TITLE_BG, 1,
                           X11_EV_EXPOSURE | X11_EV_BUTTON_PRESS |
                               X11_EV_BUTTON_RELEASE | X11_EV_POINTER_MOTION);
  if (frame == 0) {
    x11_map_window(&c, win);
    return;
  }
  x11_add_to_save_set(&c, win);
  x11_reparent(&c, win, frame, BORDER, TITLE_H);
  /* 【点内容区也要抬升/聚焦】frame 是客户端窗口的父窗口，点在客户端区域内
   * 的按键只会送给客户端自己（frame 收不到），于是"点一下别的窗口把它抬到
   * 前面"就失效——openbox 这类 WM 的核心手感。这里给客户端窗口补选
   * ButtonPress：event-mask 是【按客户端】记的，只影响本进程收到的事件，
   * 不会抢走客户端自己的按键。 */
  x11_select_input(&c, win, X11_EV_BUTTON_PRESS);
  idx = ncls;
  cls[ncls].win = win;
  cls[ncls].frame = frame;
  cls[ncls].w = w;
  cls[ncls].h = h;
  ncls++;
  x11_map_window(&c, win);
  x11_map_window(&c, frame);
  x11_raise(&c, frame);
  x11_set_focus(&c, win);
  draw_frame(idx);
  update_client_list();
}

static void close_client(int idx) {
  uint32_t protos[8];
  uint32_t win = cls[idx].win;
  int n = x11_get_property32(&c, win, atom_protocols, protos, 8);
  int i;
  for (i = 0; i < n; i++) {
    if (protos[i] == atom_delete) {
      uint8_t ev[32];
      memset(ev, 0, sizeof(ev));
      ev[0] = X11_CLIENT_MESSAGE;
      ev[1] = 32; /* format */
      /* data 从偏移 12 开始：window, type, d0(=WM_DELETE_WINDOW), d1 */
      ev[4] = (uint8_t)(win & 0xff);
      ev[5] = (uint8_t)((win >> 8) & 0xff);
      ev[6] = (uint8_t)((win >> 16) & 0xff);
      ev[7] = (uint8_t)((win >> 24) & 0xff);
      ev[8] = (uint8_t)(atom_protocols & 0xff);
      ev[9] = (uint8_t)((atom_protocols >> 8) & 0xff);
      ev[10] = (uint8_t)((atom_protocols >> 16) & 0xff);
      ev[11] = (uint8_t)((atom_protocols >> 24) & 0xff);
      ev[12] = (uint8_t)(atom_delete & 0xff);
      ev[13] = (uint8_t)((atom_delete >> 8) & 0xff);
      ev[14] = (uint8_t)((atom_delete >> 16) & 0xff);
      ev[15] = (uint8_t)((atom_delete >> 24) & 0xff);
      x11_send_event(&c, win, 0, ev);
      return;
    }
  }
  /* 客户端不支持 WM_DELETE_WINDOW：直接断它 */
  x11_kill_client(&c, win);
}

static void on_button(const uint8_t* ev) {
  uint32_t win = x11_ev_u32(ev, 12); /* 事件窗口：frame 或（内容区的）客户端窗口 */
  uint32_t frame;
  int button = ev[1]; /* 1=左 2=中 3=右 4/5=滚轮 */
  int ex = (int16_t)(ev[24] | (ev[25] << 8));
  int ey = (int16_t)(ev[26] | (ev[27] << 8));
  int rx = (int16_t)(ev[20] | (ev[21] << 8));
  int ry = (int16_t)(ev[22] | (ev[23] << 8));
  int idx = find_frame(win);
  int fx = 0, fy = 0, fw = 0;

  if (idx < 0) {
    idx = find_win(win); /* 点在客户端内容区：先抬升/聚焦，不做拖动 */
  }
  if (idx < 0) {
    return;
  }
  frame = cls[idx].frame;
  x11_raise(&c, frame);
  x11_set_focus(&c, cls[idx].win);
  /* 只有左键参与拖动/关闭。右键留给后续的窗口菜单，滚轮忽略。 */
  if (button != 1 || win != frame) {
    return;
  }
  x11_get_geometry(&c, frame, &fx, &fy, &fw, NULL);
  if (ey >= 0 && ey < TITLE_H) {
    if (ex >= fw - TITLE_H) {
      close_client(idx);
      return;
    }
    drag_on = 1;
    drag_frame = frame;
    drag_dx = rx - fx;
    drag_dy = ry - fy;
    x11_grab_pointer(&c, frame,
                     X11_EV_BUTTON_PRESS | X11_EV_BUTTON_RELEASE |
                         X11_EV_POINTER_MOTION);
  }
}

static void on_motion(const uint8_t* ev) {
  int rx, ry;
  if (!drag_on) {
    return;
  }
  rx = (int16_t)(ev[20] | (ev[21] << 8));
  ry = (int16_t)(ev[22] | (ev[23] << 8));
  x11_configure(&c, drag_frame, rx - drag_dx, ry - drag_dy, -1, -1);
}

static void handle(const uint8_t* ev) {
  switch (ev[0]) {
    case EV_MAP_REQUEST:
      /* MapRequest 的布局是 parent@4 + window@8：window 在偏移 8。
       * 取偏移 4 会拿到 root，manage() 去 reparent 根窗口直接 BadMatch，
       * 客户端窗口永远不会被 map（进程活着但窗口不出现）。 */
      manage(x11_ev_u32(ev, 8));
      break;
    case EV_CONFIGURE_REQUEST: {
      uint32_t win = x11_ev_u32(ev, 8);
      int x = (int16_t)(ev[16] | (ev[17] << 8));
      int y = (int16_t)(ev[18] | (ev[19] << 8));
      int w = (int16_t)(ev[20] | (ev[21] << 8));
      int h = (int16_t)(ev[22] | (ev[23] << 8));
      int idx = find_win(win);
      if (idx < 0) {
        break;
      }
      if (w > 0 && h > 0) {
        cls[idx].w = w;
        cls[idx].h = h;
        /* 客户端在 frame 内部的固定槽位是 (BORDER, TITLE_H)：
         * 位置不能按 (0,0) 摆，否则会盖住标题栏。 */
        x11_configure(&c, win, BORDER, TITLE_H, w, h);
        x11_configure(&c, cls[idx].frame, -1, -1, w + 2 * BORDER,
                      h + TITLE_H + BORDER);
        draw_frame(idx);
      } else if (x != 0 || y != 0) {
        int fx = 0, fy = 0;
        x11_get_geometry(&c, cls[idx].frame, &fx, &fy, NULL, NULL);
        x11_configure(&c, cls[idx].frame, fx + x, fy + y, -1, -1);
      }
      break;
    }
    case EV_UNMAP_NOTIFY: {
      uint32_t win = x11_ev_u32(ev, 8);
      int idx = find_win(win);
      if (idx >= 0) {
        unmanage(idx, 1);
      }
      break;
    }
    case EV_DESTROY_NOTIFY: {
      uint32_t win = x11_ev_u32(ev, 8);
      int idx = find_win(win);
      if (idx >= 0) {
        unmanage(idx, 0); /* 客户端已消失，frame 靠自己的 Unmap 收尾 */
      }
      break;
    }
    case X11_EXPOSE: {
      int idx = find_frame(x11_ev_u32(ev, 4));
      if (idx >= 0) {
        draw_frame(idx);
      }
      break;
    }
    case X11_BUTTON_PRESS:
      on_button(ev);
      break;
    case X11_CLIENT_MESSAGE: {
      /* dock 点任务按钮 → 发 _NET_ACTIVE_WINDOW 给根窗口，
       * 这里约定 data[0] 就是目标窗口（不走 EWMH 的 3 字段格式）。 */
      uint32_t type = x11_ev_u32(ev, 8);
      uint32_t target = x11_ev_u32(ev, 12);
      if (type == atom_active) {
        activate(target);
      }
      break;
    }
    case EV_MOTION_NOTIFY:
      on_motion(ev);
      break;
    case X11_BUTTON_RELEASE:
      if (drag_on) {
        drag_on = 0;
        x11_ungrab_pointer(&c);
      }
      break;
    default:
      break;
  }
}

int main(int argc, char* argv[]) {
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  uint8_t ev[32];
  uint32_t kids[64];
  int n, i;

  (void)argc;
  (void)argv;
  if (x11_connect(&c, NULL) < 0) {
    return 1;
  }
  /* 根窗口先挂上箭头光标：frame 是之后创建的，会继承这个 cursor。
   * 不设的话在精简镜像里就是"隐形光标"（X 的默认光标取不到核心字体）。 */
  if (x11_install_arrow_cursor(&c) < 0) {
    fprintf(stderr, "wm: 设置光标失败（屏幕上看不到指针）\n");
  } else {
    printf("wm: 光标已挂到根窗口 0x%x\n", c.root);
  }
  gc = x11_id(&c);
  font = x11_id(&c);
  if (x11_open_font(&c, font, "fixed") < 0) {
    fprintf(stderr, "wm: 打开 fixed 字体失败\n");
    return 1;
  }
  x11_create_gc(&c, gc, c.root);
  x11_set_gc_font(&c, gc, font);

  atom_protocols = x11_intern_atom(&c, "WM_PROTOCOLS");
  atom_delete = x11_intern_atom(&c, "WM_DELETE_WINDOW");
  atom_name = x11_intern_atom(&c, "WM_NAME");
  atom_client_list = x11_intern_atom(&c, X11_NET_CLIENT_LIST);
  atom_active = x11_intern_atom(&c, X11_NET_ACTIVE_WINDOW);

  /* 接管根窗口：SubstructureRedirect(0x100000) | SubstructureNotify(0x80000) */
  x11_change_attrs(&c, c.root, 0x800, 0x180000);
  x11_sync(&c);
  printf("wm: 已接管 %dx%d 根窗口\n", c.w, c.h);

  /* 接手已经存在的顶层窗口（先起客户端后起 WM 的场景） */
  n = x11_query_children(&c, c.root, kids, 64);
  for (i = 0; i < n; i++) {
    int map_state = 0, override = 0;
    if (x11_get_attrs(&c, kids[i], &map_state, &override, NULL) == 0 &&
        !override && map_state != 1 /* 不是 IsUnmapped */) {
      manage(kids[i]);
    }
  }

  for (;;) {
    struct pollfd pfd;
    pfd.fd = c.fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, 2000) <= 0) {
      continue;
    }
    while (x11_next_event(&c, ev) == 1) {
      handle(ev);
    }
  }
  return 0;
}
