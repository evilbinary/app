/*******************************************************************
 * wm — 自研极简窗口管理器（替代 openbox）。
 *
 * 只做"能把窗口摆出来并拖动/关闭"的最小实现（reparenting WM）：
 *
 *   1. 抢根窗口 SubstructureRedirect，接管所有顶层窗口
 *   2. MapRequest → 建一个带标题栏的 frame，ReparentWindow 把客户端装进去
 *      再把 frame 映射出来（客户端自己不用改代码）
 *   3. 标题栏：左边显示 WM_NAME，右边三个按钮（最小化 / 最大化 / 关闭）；
 *      点标题栏空白处拖动窗口（最大化状态下不拖）
 *   4. ConfigureRequest 按客户端请求改 frame 尺寸（最大化期间挡回全屏）；
 *      客户端自己的 Unmap/Destroy 收 frame
 *   5. 启动时把已经存在的顶层窗口（先起客户端后起 WM 的情况）也接管
 *
 * 刻意不做的：EWMH 全量（只发布 _NET_CLIENT_LIST、接收 _NET_ACTIVE_WINDOW）、
 * 焦点跟随鼠标、多桌面、窗口动画。面板(panel) 与背景(wall) 是
 * override-redirect 窗口，不会被这里接管。
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
/* 边框也是"缩放手柄"：窗口的四条边框宽度 = BORDER，鼠标落在边框上就
 * 进入缩放，所以不能太细（原来 2px 基本抓不住）。 */
#define BORDER 5
#define EDGE BORDER /* 抓边宽度 */
#define MIN_CW 32    /* 内容区最小尺寸 */
#define MIN_CH 24
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
  int w, h;      /* 客户端内容区尺寸 */
  int ox, oy;    /* 最大化前的 frame 位置（还原用） */
  int ow, oh;    /* 最大化前的内容区尺寸 */
  int maxed;     /* 1 = 最大化中 */
  int minimized; /* 1 = 已图标化（frame 被 unmap） */
  char title[64]; /* WM_NAME 缓存（接管时读一次，重绘不再 round-trip） */
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
/* ReparentWindow 会给客户端窗口发一条 UnmapNotify（我们刚选了 StructureNotify），
 * 这条不能当成"客户端自己被隐藏"，否则刚接管的窗口会立刻被 unmanage。 */
static uint32_t ignore_unmap_win;

/* 缩放状态：resize_edge 是位掩码 1=左 2=右 4=上 8=下 */
#define EDGE_LEFT 1
#define EDGE_RIGHT 2
#define EDGE_TOP 4
#define EDGE_BOTTOM 8
static int resize_on;
static uint32_t resize_frame;
static int resize_edge;
static int rs_x, rs_y, rs_w, rs_h; /* 按下瞬间的 frame 几何 */
static int rs_rx, rs_ry;           /* 按下瞬间的鼠标 root 坐标 */

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

/* 开始缩放：记下按下瞬间的 frame 几何与鼠标位置，后面全靠增量算 */
static void start_resize(int idx, int edge, int rx, int ry) {
  int fx = 0, fy = 0, fw = 0, fh = 0;
  x11_get_geometry(&c, cls[idx].frame, &fx, &fy, &fw, &fh);
  if (fw <= 0 || fh <= 0) {
    return;
  }
  resize_on = 1;
  resize_frame = cls[idx].frame;
  resize_edge = edge;
  rs_x = fx;
  rs_y = fy;
  rs_w = fw;
  rs_h = fh;
  rs_rx = rx;
  rs_ry = ry;
  /* 抓住指针：鼠标移出 frame（甚至移出屏幕边缘）也要继续收到 motion */
  x11_grab_pointer(&c, cls[idx].frame,
                   X11_EV_BUTTON_PRESS | X11_EV_BUTTON_RELEASE |
                       X11_EV_POINTER_MOTION);
}

/* 按鼠标位移把命中的边往外/往里拖，同步改客户端内容区尺寸 */
static void on_resize(int rx, int ry) {
  int idx;
  int dx, dy;
  int nx, ny, nw, nh, cw, ch;

  if (!resize_on) {
    return;
  }
  idx = find_frame(resize_frame);
  if (idx < 0) { /* 缩放途中窗口被关了 */
    resize_on = 0;
    return;
  }
  dx = rx - rs_rx;
  dy = ry - rs_ry;
  nx = rs_x;
  ny = rs_y;
  nw = rs_w;
  nh = rs_h;
  if (resize_edge & EDGE_LEFT) {
    nx = rs_x + dx;
    nw = rs_w - dx;
  }
  if (resize_edge & EDGE_RIGHT) {
    nw = rs_w + dx;
  }
  if (resize_edge & EDGE_TOP) {
    ny = rs_y + dy;
    nh = rs_h - dy;
  }
  if (resize_edge & EDGE_BOTTOM) {
    nh = rs_h + dy;
  }
  /* 最小尺寸：拖过头就把对边钉住（否则窗口会反向移动） */
  if (nw < MIN_CW + 2 * BORDER) {
    if (resize_edge & EDGE_LEFT) {
      nx = rs_x + rs_w - (MIN_CW + 2 * BORDER);
    }
    nw = MIN_CW + 2 * BORDER;
  }
  if (nh < MIN_CH + TITLE_H + BORDER) {
    if (resize_edge & EDGE_TOP) {
      ny = rs_y + rs_h - (MIN_CH + TITLE_H + BORDER);
    }
    nh = MIN_CH + TITLE_H + BORDER;
  }
  cw = nw - 2 * BORDER;
  ch = nh - TITLE_H - BORDER;
  cls[idx].w = cw;
  cls[idx].h = ch;
  /* frame 的 x/y 允许为负（窗口可以贴着屏幕左边/上边），所以不能再用
   * "负数 = 不改"的旧约定，统一走 X11_KEEP 哨兵。 */
  x11_configure(&c, cls[idx].frame, nx, ny, nw, nh);
  x11_configure(&c, cls[idx].win, BORDER, TITLE_H, cw, ch);
}

static void draw_frame(int idx); /* 前置声明：下面几个几何操作要重绘 */

/* 把 frame 摆到 (fx,fy)，内容区给客户端的尺寸 cw×ch。
 * frame 比内容区多出：左右各 BORDER，上面 TITLE_H，下面 BORDER。 */
static void apply_size(int idx, int fx, int fy, int cw, int ch) {
  if (cw < 1 || ch < 1) {
    return;
  }
  cls[idx].w = cw;
  cls[idx].h = ch;
  x11_configure(&c, cls[idx].frame, fx, fy, cw + 2 * BORDER,
                ch + TITLE_H + BORDER);
  x11_configure(&c, cls[idx].win, BORDER, TITLE_H, cw, ch);
}

/* 最大化 / 还原：最大化铺满整屏，还原回原来的位置与尺寸 */
static void toggle_max(int idx) {
  if (cls[idx].maxed) {
    cls[idx].maxed = 0;
    apply_size(idx, cls[idx].ox, cls[idx].oy, cls[idx].ow, cls[idx].oh);
  } else {
    int fx = 0, fy = 0, fw = 0, fh = 0;
    x11_get_geometry(&c, cls[idx].frame, &fx, &fy, &fw, &fh);
    if (fw <= 0 || fh <= 0) {
      return;
    }
    cls[idx].ox = fx;
    cls[idx].oy = fy;
    cls[idx].ow = fw - 2 * BORDER;
    cls[idx].oh = fh - TITLE_H - BORDER;
    cls[idx].maxed = 1;
    apply_size(idx, 0, 0, c.w - 2 * BORDER, c.h - TITLE_H - BORDER);
  }
  draw_frame(idx);
}

/* 最小化：把整个 frame 收起来（客户端窗口跟着不显示）。
 * 恢复走 activate()：dock 的任务按钮发 _NET_ACTIVE_WINDOW 给根窗口。 */
static void minimize_win(int idx) {
  cls[idx].minimized = 1;
  x11_unmap_window(&c, cls[idx].frame);
}

static void restore_win(int idx) {
  if (!cls[idx].minimized) {
    return;
  }
  cls[idx].minimized = 0;
  x11_map_window(&c, cls[idx].frame);
  draw_frame(idx);
}

static void draw_frame(int idx) {
  char title[64];
  int fw, fh;
  int i;
  uint32_t frame = cls[idx].frame;

  /* 【不在这里 round-trip】尺寸用记录的客户端内容区尺寸算（frame 只比它
   * 多 BORDER/TITLE_H），标题用接管时缓存的 WM_NAME。原来每次 Expose 都要
   * GetGeometry + GetProperty 两个同步往返：窗口一多、事件一密，wm 就卡在
   * 这两个往返上，Xorg 的事件排队往外写不动，指针跟着一起卡。 */
  fw = cls[idx].w + 2 * BORDER;
  fh = cls[idx].h + TITLE_H + BORDER;
  if (fw <= 0 || fh <= 0) {
    return;
  }
  x11_set_gc(&c, gc, 0x0C /*FG|BG*/, TITLE_BG, TITLE_BG);
  x11_fill_rect(&c, frame, gc, 0, 0, fw, TITLE_H);

  /* 右侧三个按钮（各占 TITLE_H 宽，从左到右：最小化 / 最大化 / 关闭）。
   * frame 太窄（< 4*TITLE_H）时放不下，只画背景，按钮区让给拖动。 */
  if (fw >= 4 * TITLE_H) {
    int bx = fw - 3 * TITLE_H;
    /* 最小化：一条横杠 */
    x11_set_gc(&c, gc, 0x0C, TITLE_FG, TITLE_BG);
    x11_fill_rect(&c, frame, gc, bx + 6, TITLE_H / 2, TITLE_H - 12, 2);
    /* 最大化：空心方框（四条 2px 边） */
    {
      int mx = bx + TITLE_H + 5;
      int my = 5;
      int mw = TITLE_H - 10;
      int mh = TITLE_H - 10;
      x11_fill_rect(&c, frame, gc, mx, my, mw, 2);
      x11_fill_rect(&c, frame, gc, mx, my + mh - 2, mw, 2);
      x11_fill_rect(&c, frame, gc, mx, my, 2, mh);
      x11_fill_rect(&c, frame, gc, mx + mw - 2, my, 2, mh);
    }
    /* 关闭：红底 + x */
    x11_set_gc(&c, gc, 0x0C, CLOSE_BG, CLOSE_BG);
    x11_fill_rect(&c, frame, gc, fw - TITLE_H, 2, TITLE_H - 4, TITLE_H - 4);
    x11_set_gc(&c, gc, 0x0C, TITLE_FG, CLOSE_BG);
    x11_image_text8(&c, frame, gc, fw - TITLE_H + 6, TITLE_H - 5, "x", 1);
  }

  memcpy(title, cls[idx].title, sizeof(title));
  if (title[0] == '\0') {
    strcpy(title, "(no name)");
  }
  /* 标题栏文字，留出右侧关闭按钮的位置。
   * 【必须同时按缓冲区长度截断】原来只按 "frame 宽度能放几个字" 算上界：
   * frame 宽度一旦是垃圾值（服务器回了个 Error 被当成 GetGeometry 回复、
   * 或客户端设了很大的初始尺寸），循环就会越过 title[64] 一路写下去，
   * 把 wm 自己的栈踩烂 —— 之后发出的请求内容全是随机的，服务器那边
   * 会按错位的数据解析。 */
  {
    /* 右侧留给按钮区（窄窗口没有按钮，只留 4px 边距） */
    int reserve = (fw >= 4 * TITLE_H) ? 3 * TITLE_H + 8 : 8;
    int lim = (fw - reserve) / 6;
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
  /* 从最小化里恢复（dock 任务按钮 / 再次启动同一个程序） */
  restore_win(idx);
  /* 已最大化就先还原再抬升？不：最大化窗口抬升后仍是最大化 */
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
  /* frame 只选按键与 Expose：PointerMotion 交给拖动/缩放时的 GrabPointer。
   * 选了 PointerMotion 的话，鼠标只要划过标题栏/边框，Xorg 就要为每个像素
   * 生成一条 MotionNotify 推给我们，窗口一多就是持续的事件流量。 */
  frame = x11_create_window(&c, x - BORDER, y - TITLE_H, w + 2 * BORDER,
                           h + TITLE_H + BORDER, TITLE_BG, 1,
                           X11_EV_EXPOSURE | X11_EV_BUTTON_PRESS |
                               X11_EV_BUTTON_RELEASE);
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
  /* 【必须同时选 StructureNotify】否则客户端自己 unmap/退出时我们收不到
   * 任何事件（frame 是它的父窗口，root 的 SubstructureNotify 只报 root 的
   * 直接子窗口），点 × 关掉程序后会留下一个空标题条僵尸窗口。 */
  x11_select_input(&c, win, X11_EV_BUTTON_PRESS | X11_EV_STRUCTURE);
  ignore_unmap_win = win; /* 紧接着的 Reparent 会产生一条 UnmapNotify，跳过它 */
  idx = ncls;
  cls[ncls].win = win;
  cls[ncls].frame = frame;
  cls[ncls].w = w;
  cls[ncls].h = h;
  cls[ncls].ox = cls[ncls].oy = 0;
  cls[ncls].ow = w;
  cls[ncls].oh = h;
  cls[ncls].maxed = 0;
  cls[ncls].minimized = 0;
  /* 标题缓存一次：之后每次 Expose 重绘直接用，不再 GetProperty */
  cls[ncls].title[0] = '\0';
  x11_get_property_str(&c, win, atom_name, cls[ncls].title,
                       (int)sizeof(cls[ncls].title));
  if (cls[ncls].title[0] == '\0') {
    strcpy(cls[ncls].title, "(no name)");
  }
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
  int fx = 0, fy = 0, fw = 0, fh = 0;

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
  x11_get_geometry(&c, frame, &fx, &fy, &fw, &fh);
  /* 1) 标题栏右侧三个按钮：从左到右 最小化 / 最大化 / 关闭，各 TITLE_H 宽 */
  if (ey >= 0 && ey < TITLE_H && fw >= 4 * TITLE_H &&
      ex >= fw - 3 * TITLE_H) {
    int which = (ex - (fw - 3 * TITLE_H)) / TITLE_H;
    if (which <= 0) {
      minimize_win(idx);
    } else if (which == 1) {
      toggle_max(idx);
    } else {
      close_client(idx);
    }
    return;
  }
  if (fw <= 0 || fh <= 0) {
    return;
  }
  /* 2) 命中边框 → 缩放。四条边框宽度都是 BORDER，正是 frame 自己（没被
   * 客户端窗口覆盖）的区域，所以左/上/右/下都能抓到。最大化时不缩放。 */
  if (!cls[idx].maxed) {
    int edge = 0;
    if (ex < EDGE) {
      edge |= EDGE_LEFT;
    }
    if (ex >= fw - EDGE) {
      edge |= EDGE_RIGHT;
    }
    if (ey < EDGE) {
      edge |= EDGE_TOP;
    }
    if (ey >= fh - EDGE) {
      edge |= EDGE_BOTTOM;
    }
    if (edge != 0) {
      start_resize(idx, edge, rx, ry);
      return;
    }
  }
  /* 3) 标题栏空白处 → 拖动 */
  if (ey >= 0 && ey < TITLE_H) {
    if (cls[idx].maxed) { /* 最大化状态不允许拖动 */
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
  int rx = (int16_t)(ev[20] | (ev[21] << 8));
  int ry = (int16_t)(ev[22] | (ev[23] << 8));
  if (resize_on) {
    on_resize(rx, ry);
    return;
  }
  if (!drag_on) {
    return;
  }
  x11_configure(&c, drag_frame, rx - drag_dx, ry - drag_dy, X11_KEEP, X11_KEEP);
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
      /* 最大化期间不接受客户端的尺寸/位置请求：重新套回全屏。
       * （xeyes/xclock 这类程序 resize 时会一路 ConfigureRequest，
       * 不挡回去窗口会从最大化状态里"缩"出来。） */
      if (cls[idx].maxed) {
        apply_size(idx, 0, 0, c.w - 2 * BORDER, c.h - TITLE_H - BORDER);
        draw_frame(idx);
        break;
      }
      if (w > 0 && h > 0) {
        cls[idx].w = w;
        cls[idx].h = h;
        /* 客户端在 frame 内部的固定槽位是 (BORDER, TITLE_H)：
         * 位置不能按 (0,0) 摆，否则会盖住标题栏。 */
        x11_configure(&c, win, BORDER, TITLE_H, w, h);
        x11_configure(&c, cls[idx].frame, X11_KEEP, X11_KEEP,
                      w + 2 * BORDER, h + TITLE_H + BORDER);
        draw_frame(idx);
      } else if (x != 0 || y != 0) {
        int fx = 0, fy = 0;
        x11_get_geometry(&c, cls[idx].frame, &fx, &fy, NULL, NULL);
        x11_configure(&c, cls[idx].frame, fx + x, fy + y, X11_KEEP,
                      X11_KEEP);
      }
      break;
    }
    case EV_UNMAP_NOTIFY: {
      uint32_t win = x11_ev_u32(ev, 8);
      int idx;
      if (win == ignore_unmap_win) {
        ignore_unmap_win = 0; /* Reparent 的副作用，忽略一次 */
        break;
      }
      idx = find_win(win);
      if (idx >= 0) {
        unmanage(idx, 1);
      }
      break;
    }
    case EV_DESTROY_NOTIFY: {
      uint32_t win = x11_ev_u32(ev, 8);
      int idx = find_win(win);
      if (idx >= 0) {
        /* 客户端窗口已经没了，但它留下的 frame 不会自己消失（没人会去
         * unmap 它）—— 必须连同 frame 一起销毁，否则点 × 关掉程序后桌上
         * 会留一个空标题条僵尸窗口。 */
        unmanage(idx, 1);
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
      if (resize_on) {
        int ridx = find_frame(resize_frame);
        resize_on = 0;
        x11_ungrab_pointer(&c);
        /* 缩放过程中不逐帧重绘（标题栏会被拉长/压短好几遍），松手时画一次 */
        if (ridx >= 0) {
          draw_frame(ridx);
        }
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
  if (x11_sync(&c) < 0) {
    fprintf(stderr, "wm: 接管根窗口失败（是不是还有别的 WM 在跑？）\n");
  } else {
    printf("wm: 已接管 %dx%d 根窗口\n", c.w, c.h);
  }

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
