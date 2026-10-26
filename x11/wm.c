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
#define EV_MAP_NOTIFY 19
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
/* 拖动目标位置：同一批 MotionNotify 只发一条 ConfigureWindow（见 on_motion） */
static int drag_pending;
static int drag_px, drag_py;
/* ReparentWindow 会给客户端窗口发一条 UnmapNotify（我们刚选了 StructureNotify），
 * 这条不能当成"客户端自己被隐藏"，否则刚接管的窗口会立刻被 unmanage。 */
static uint32_t ignore_unmap_win;
/* 【焦点要等窗口真 visible 了再设】SetInputFocus 要求目标是 viewable 的，
 * 而 MapWindow 是异步的：manage() 里 map 完立刻设焦点，服务器还在处理
 * map、窗口仍是 IsUnmapped ⇒ BadMatch(code=8 major=42)，键盘输入进不去。
 * 记下待设的窗口，等它自己的 MapNotify 到了再设（StructureNotify 已选）。 */
static uint32_t pending_focus;

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
  /* 抓住指针：鼠标移出 frame（甚至移出屏幕边缘）也要继续收到 motion。
   * 抓不到就别进缩放状态：否则 motion 全送给指针下的客户端（xeyes 会跟着
   * 每像素重绘），我们却一直等不到事件 —— 又卡又没反应。 */
  if (x11_grab_pointer(&c, cls[idx].frame,
                       X11_EV_BUTTON_PRESS | X11_EV_BUTTON_RELEASE |
                           X11_EV_POINTER_MOTION) < 0) {
    resize_on = 0;
  }
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
  printf("wm: minimize win=0x%x frame=0x%x\n", cls[idx].win, cls[idx].frame);
  x11_unmap_window(&c, cls[idx].frame);
}

static void restore_win(int idx) {
  if (!cls[idx].minimized) {
    return;
  }
  cls[idx].minimized = 0;
  printf("wm: restore win=0x%x frame=0x%x\n", cls[idx].win, cls[idx].frame);
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
   * frame 太窄（< 4*TITLE_H）时放不下，只画背景，按钮区让给拖动。
   * 【图标一律按"格子中心"算】早先每个图标各写一套硬编码偏移，关闭按钮
   * 更是直接用核心字体的 'x' 字符画：字宽 6px、基线在 TITLE_H-5，字形墨迹
   * 又不占满格 ⇒ 叉号被顶到红色按钮的右上角，怎么调都看着不居中。 */
  if (fw >= 4 * TITLE_H) {
    int c0 = fw - 3 * TITLE_H + TITLE_H / 2; /* 三个格的中心 x（与命中区同一网格） */
    int c1 = c0 + TITLE_H;
    int c2 = c1 + TITLE_H;
    int cy = TITLE_H / 2;
    int k;
    x11_set_gc(&c, gc, 0x0C, TITLE_FG, TITLE_BG);
    /* 最小化：8x2 横杠，居中 */
    x11_fill_rect(&c, frame, gc, c0 - 4, cy - 1, 8, 2);
    /* 最大化：10x10 空心方框（四条 2px 边），居中 */
    {
      int mx = c1 - 5;
      int my = cy - 5;
      x11_fill_rect(&c, frame, gc, mx, my, 10, 2);
      x11_fill_rect(&c, frame, gc, mx, my + 8, 10, 2);
      x11_fill_rect(&c, frame, gc, mx, my, 2, 10);
      x11_fill_rect(&c, frame, gc, mx + 8, my, 2, 10);
    }
    /* 关闭：16x16 红底 + 8x8 对角叉，都居中于格子 */
    x11_set_gc(&c, gc, 0x0C, CLOSE_BG, CLOSE_BG);
    x11_fill_rect(&c, frame, gc, c2 - 8, cy - 8, 16, 16);
    x11_set_gc(&c, gc, 0x0C, TITLE_FG, CLOSE_BG);
    for (k = 0; k < 4; k++) { /* 每条对角线 4 段 2x2，正反各一遍 */
      x11_fill_rect(&c, frame, gc, c2 - 4 + k * 2, cy - 4 + k * 2, 2, 2);
      x11_fill_rect(&c, frame, gc, c2 + 2 - k * 2, cy - 4 + k * 2, 2, 2);
    }
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
  printf("wm: activate win=0x%x idx=%d\n", win, idx);
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
  if (pending_focus == cls[idx].win) {
    pending_focus = 0; /* 窗口没了就别等它的 MapNotify 了 */
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
    printf("wm: manage skip win=0x%x (dup/full)\n", win);
    x11_map_window(&c, win);
    return;
  }
  if (x11_get_attrs(&c, win, &map_state, &override, NULL) < 0) {
    /* 【必须兜底 map】这里若直接 return，这条被重定向扣住的 MapRequest 就
     * 再也没人满足了 ⇒ 客户端进程活着、窗口永远不出现。 */
    printf("wm: get_attrs fail win=0x%x -> map as-is\n", win);
    x11_map_window(&c, win);
    return;
  }
  if (override) { /* 菜单/面板这类不该被接管 */
    printf("wm: override win=0x%x -> map as-is\n", win);
    x11_map_window(&c, win);
    return;
  }
  if (x11_get_geometry(&c, win, &x, &y, &w, &h) < 0 || w <= 0 || h <= 0) {
    printf("wm: geometry fail win=0x%x (%d,%d %dx%d) -> map as-is\n", win, x, y,
           w, h);
    x11_map_window(&c, win);
    return;
  }
  /* 【标题栏别跑到屏幕外】客户端自己的位置在 0,0 时 frame 的坐标是
   * -BORDER / -TITLE_H：标题栏、关闭按钮全落在负坐标上 ⇒ 拖不动也点不到
   * （表现为"窗口出现在左上角但没法操作"）。左上贴边即可。 */
  if (x < BORDER) {
    x = BORDER;
  }
  if (y < TITLE_H) {
    y = TITLE_H;
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
  /* 【事件掩码必须分两步选，一次选会被 BadAccess 连坐】
   * 需求有两块：
   *   1) StructureNotify：客户端自己 unmap/退出时要收到事件（frame 是它的
   *      父窗口，root 的 SubstructureNotify 只报 root 的直接子窗口），不选
   *      就会在点 × 之后留下空标题条僵尸窗口。
   *   2) ButtonPress：点在客户端内容区也能抬升/聚焦（frame 收不到子窗口上
   *      的按键）。event-mask 按客户端记，只影响本进程收到的事件，不会抢走
   *      客户端自己的按键。
   * 【坑】在"自己也选了 ButtonPress"的客户端窗口上（xcalc/xterm 这类）再选
   * ButtonPress，Xorg/Xephyr/Xvfb 一致回 BadAccess，而且**整条请求作废**
   * ⇒ 连 StructureNotify 都没选上（实测 xcalc 关掉后留下僵尸标题条）。
   * 所以先单独选 StructureNotify（一定成功），再尽力补 ButtonPress；补不上
   * 就退化成"点标题栏抬升"。 */
  x11_select_input(&c, win, X11_EV_STRUCTURE);
  (void)x11_select_input_optional(&c, win,
                                  X11_EV_BUTTON_PRESS | X11_EV_STRUCTURE);
  /* 【backing store：默认关，实测是坑不是药】
   * 原意：让服务器保存"被盖住"的内容，窗口露出时直接贴回，不给客户端发
   * Expose，省掉重画。
   * 实测两个问题（macOS XQuartz + Xephyr）：
   *   1. 正确性：给客户端窗口设 WhenMapped 后，客户端在 reparent 的
   *      unmap→map 期间画好的内容被丢弃，而且**不再补发 Expose** ⇒ 窗口
   *      永久空白（自研 xeyes 实测整窗全空，去掉就正常）。
   *   2. 性能：每次遮挡/暴露服务器要把整窗内容 memcpy 进出备份缓冲，
   *      拖一步的端到端延迟 0.65ms → 1.97ms（XTEST 实测）。
   * 所以默认不设；WM_BACKING_STORE=1 时才打开，留给慢服务器做对比。 */
  if (getenv("WM_BACKING_STORE") != NULL) {
    x11_change_attrs(&c, frame, 0x40 /* CWBackingStore */, 1 /* WhenMapped */);
    x11_change_attrs(&c, win, 0x40, 1);
  }
  ignore_unmap_win = win; /* 紧接着的 Reparent 会产生一条 UnmapNotify，跳过它 */
  idx = ncls;
  cls[ncls].win = win;
  cls[ncls].frame = frame;
  cls[ncls].w = w;
  cls[ncls].h = h;
  /* 记 frame 的当前位置（最大化后还原用），与 create_window 传的坐标一致 */
  cls[ncls].ox = x - BORDER;
  cls[ncls].oy = y - TITLE_H;
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
  pending_focus = win; /* 等 MapNotify：此刻窗口还不可见，直接设焦点会 BadMatch */
  draw_frame(idx);
  update_client_list();
  printf("wm: manage ok win=0x%x geo=%d,%d %dx%d frame=0x%x ncls=%d\n", win, x, y,
         w, h, frame, ncls);
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
    /* 抓不到指针就不要进拖动状态（见 start_resize 的说明） */
    if (x11_grab_pointer(&c, frame,
                         X11_EV_BUTTON_PRESS | X11_EV_BUTTON_RELEASE |
                             X11_EV_POINTER_MOTION) < 0) {
      return;
    }
    drag_on = 1;
    drag_frame = frame;
    drag_dx = rx - fx;
    drag_dy = ry - fy;
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
  /* 【合并同一批 Motion】拖动时鼠标每动一个像素服务器就发一条 MotionNotify，
   * 而每条都发一次 ConfigureWindow 的话：服务器要真正移动窗口 → 露出下层
   * 窗口 → 那些窗口收到 Expose 各自重绘 → X 队列暴涨，鼠标跟着顿（窗口越多
   * 越明显）。这里只记目标位置，等这一批事件处理完，由主循环统一发一次。 */
  drag_px = rx - drag_dx;
  drag_py = ry - drag_dy;
  drag_pending = 1;
}

/* 把合并后的拖动位置真正发出去（主循环每处理完一批事件调一次）。 */
static void flush_drag(void) {
  if (drag_pending) {
    drag_pending = 0;
    x11_configure(&c, drag_frame, drag_px, drag_py, X11_KEEP, X11_KEEP);
  }
}

static void handle(const uint8_t* ev) {
  /* 【必须掩掉 send_event 位】别的客户端用 SendEvent 发来的事件（dock 的
   * _NET_ACTIVE_WINDOW ClientMessage 就是），线上 type 的最高位是 1：33 变成
   * 0xA1=161。照着 33 匹配永远命中不了 default 分支 ⇒ 点 dock 任务按钮毫无
   * 反应（最小化到 dock 的窗口再也恢复不回来）。Xlib 也是这么掩的。 */
  switch (ev[0] & 0x7f) {
    case EV_MAP_REQUEST: {
      uint32_t mw = x11_ev_u32(ev, 8);
      /* MapRequest 的布局是 parent@4 + window@8：window 在偏移 8。
       * 取偏移 4 会拿到 root，manage() 去 reparent 根窗口直接 BadMatch，
       * 客户端窗口永远不会被 map（进程活着但窗口不出现）。 */
      printf("wm: MapRequest win=0x%x\n", mw);
      manage(mw);
      break;
    }
    case EV_CONFIGURE_REQUEST: {
      /* 【字段偏移】ConfigureRequest = type(0) stack-mode(1) seq(2) parent(4)
       * window(8) x(12) y(14) width(16) height(18) border-width(20)
       * value-mask(22)。早先按 16/18/20/22 取，等于把 width 当 x、
       * value-mask 当 height ⇒ 客户端只想改尺寸时窗口却被"按自己的宽高
       * 又挪了一次位置"（窗口一出现就跑到屏幕中间）。 */
      uint32_t win = x11_ev_u32(ev, 8);
      int x = (int16_t)(ev[12] | (ev[13] << 8));
      int y = (int16_t)(ev[14] | (ev[15] << 8));
      int w = (int16_t)(ev[16] | (ev[17] << 8));
      int h = (int16_t)(ev[18] | (ev[19] << 8));
      int mask = ev[22] | (ev[23] << 8);
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
      /* 【必须看 value-mask】只有置位的字段才有效：只发"挪位置"(CWX|CWY)
       * 的客户端，w/h 字段是 0，照着用就会被缩成 0×0。 */
      {
        int nw = (mask & 0x04) ? w : cls[idx].w; /* CWWidth */
        int nh = (mask & 0x08) ? h : cls[idx].h; /* CWHeight */
        if ((mask & 0x0c) != 0 && nw > 0 && nh > 0) {
          cls[idx].w = nw;
          cls[idx].h = nh;
          /* 客户端在 frame 内部的固定槽位是 (BORDER, TITLE_H)：
           * 位置不能按 (0,0) 摆，否则会盖住标题栏。 */
          x11_configure(&c, win, BORDER, TITLE_H, nw, nh);
          x11_configure(&c, cls[idx].frame, X11_KEEP, X11_KEEP,
                        nw + 2 * BORDER, nh + TITLE_H + BORDER);
          draw_frame(idx);
          break;
        }
      }
      if ((mask & 0x03) != 0) { /* CWX|CWY：客户端要挪位置 */
        int fx = 0, fy = 0;
        x11_get_geometry(&c, cls[idx].frame, &fx, &fy, NULL, NULL);
        x11_configure(&c, cls[idx].frame, fx + x, fy + y, X11_KEEP,
                      X11_KEEP);
      }
      break;
    }
    case EV_MAP_NOTIFY: {
      /* 客户端窗口真 visible 了：这时候设焦点才合法（见 pending_focus 注释） */
      uint32_t mw = x11_ev_u32(ev, 8);
      if (pending_focus != 0 && mw == pending_focus) {
        x11_set_focus(&c, mw);
        pending_focus = 0;
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
      /* 【合并成批到达的 Expose】Expose 的 count 字段（偏移 16）> 0 表示
       * "本窗口后面还有 Expose"，此时重绘是白画——一次拖动/抬升会生成几十条
       * Expose，逐条重绘等于把标题栏画几十遍（每遍约 20 条请求），窗口一多
       * 就把 X server 的队列撑爆，鼠标跟着一起顿。只认最后一条（count==0）。 */
      if (idx >= 0 && ev[16] == 0) {
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
        flush_drag(); /* 最后一段位移不能丢（否则松手位置差几像素） */
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

  printf("wm: 事件循环开始（已有 %d 个窗口在管）\n", ncls);
  for (;;) {
    struct pollfd pfd;
    int got = 0;
    int r;
    pfd.fd = c.fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, 2000) <= 0) {
      continue;
    }
    while ((r = x11_next_event(&c, ev)) == 1) {
      got++;
      handle(ev);
    }
    flush_drag(); /* 这一批 Motion 合并成一条 configure 再发 */
    if (r < 0) {  /* 连接断了：继续跑只会空转 */
      fprintf(stderr, "wm: 与 X 的连接断开，退出\n");
      return 1;
    }
    /* 【假唤醒让一步】本内核 poll 偶发"报就绪但读不到事件"：不让步的话这里
     * 就是 poll→空读→poll 的死循环，一核直接烧满（实测 wm 空闲 6%）。 */
    if (got == 0) {
      usleep(2000);
    }
  }
  return 0;
}
