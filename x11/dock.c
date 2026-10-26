/*******************************************************************
 * dock — 自研底部 dock（应用启动器 + 运行中窗口按钮）。
 *
 * 视觉要点（macOS 风格）：
 *   1. 窗口只有"图标组"那么大的一条居中胶囊，不再用整条不透明底栏把
 *      桌面壁纸盖住；窗口再用 SHAPE 扩展裁成圆角矩形，四角不显示 ⇒
 *      胶囊之外全是桌面背景，不会留下方角的色块。
 *   2. 按钮用图标：启动时读 /res/dock-*.bmp（tools/gen_dock_icons.py
 *      生成，图标底色与胶囊同色 ⇒ 看起来像无底框的图标）；读不到就退回
 *      彩色方块 + 字母，保证没有资源时也能用。
 *
 * 交互：
 *   左边  固定启动器：终端 / xeyes / xcalc（都是本仓库自带的客户端）
 *   右边  运行中的窗口：读根窗口 _NET_CLIENT_LIST，每个窗口一个按钮，
 *         点一下发 _NET_ACTIVE_WINDOW 给根窗口，由 wm 负责 raise + 焦点
 *
 * 每 800ms 轮询一次 _NET_CLIENT_LIST，变了就重画（窗口数量变化时同时
 * 调整胶囊宽度并保持居中）。
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
#define ICON 36
#define PAD 8
#define STEP (ICON + PAD)
#define MAX_RUN 12
#define CORNER 12 /* 胶囊圆角半径 */

#define DOCK_BG2 X11_RGB(0x2e, 0x34, 0x40)
#define DOCK_FG X11_RGB(0xec, 0xef, 0xf4)
#define RUN_ON X11_RGB(0x5e, 0x81, 0xac)

struct launcher {
  const char* letter; /* 图标缺失时的回退字母 */
  const char* exec;
  const char* icon;
  uint32_t color; /* 回退色块颜色 */
};

static struct launcher launch[] = {
    {"T", "/usr/bin/lxterminal", "dock-term.bmp", X11_RGB(0xa3, 0xbe, 0x8c)},
    {"E", "/bin/xeyes", "dock-eyes.bmp", X11_RGB(0xeb, 0xcb, 0x8b)},
    {"C", "/bin/xcalc", "dock-calc.bmp", X11_RGB(0x88, 0xc0, 0xd0)},
};
#define N_LAUNCH ((int)(sizeof(launch) / sizeof(launch[0])))
#define ICON_RUN (N_LAUNCH) /* 运行中窗口按钮用的图标下标 */

static x11_t c;
static uint32_t dock, gc, font;
static uint32_t atom_client_list, atom_active;
static uint32_t runs[MAX_RUN];
static int nruns;
static int dock_w; /* 胶囊（也是窗口）宽度 */
static x11_image_t icons[N_LAUNCH + 1]; /* 0..N_LAUNCH-1 启动器，末尾运行窗口 */

static void spawn(const char* path) {
  pid_t p = fork();
  if (p == 0) {
    execl(path, path, (char*)NULL);
    _exit(127);
  }
}

/* 胶囊宽度：图标组 + 两侧留白（窗口就只占这么大，不盖住桌面背景） */
static int calc_w(int count) {
  int w;
  if (count <= 0) {
    w = 2 * PAD;
  } else {
    w = count * ICON + (count - 1) * PAD + 2 * PAD;
    if (w > c.w) {
      w = c.w;
    }
  }
  /* 【宽度下限 = 2 倍圆角半径】胶囊是逐行算 inset 的：宽 16 时首行 inset
   * 达到 12，w-2*inset 变成负数 ⇒ 交给 ShapeRectangles 的是退化矩形，
   * Xorg 的 region 代码（pixman）会断言 "r->x1 < r->x2 was false" 刷屏。 */
  if (w < 2 * CORNER) {
    w = 2 * CORNER;
  }
  return w;
}

/* 把 w×h 的圆角矩形拆成逐行矩形（ShapeBounding 用，最多 DOCK_H 组）。
 * 逐行收边比"三段矩形"精确：横向也是整行宽度，不会有缺口。 */
static int round_rects(int16_t* r, int w, int h, int rad) {
  int y, n = 0;
  for (y = 0; y < h && n < 64; y++) {
    int dy = (y < h - 1 - y) ? y : (h - 1 - y); /* 到最近水平边的距离 */
    int inset = 0;
    int row_w;
    if (dy < rad) {
      int dz = rad - dy;
      int inner = rad * rad - dz * dz;
      int s = 0;
      while ((s + 1) * (s + 1) <= inner) {
        s++;
      }
      inset = rad - s;
    }
    row_w = w - 2 * inset;
    /* 【空/负宽度行直接不发】ShapeRectangles 里出现 x1>=x2 或 y1>=y2 的
     * 矩形，服务器拼 region 时会断言（pixman_region_append_non_o）。 */
    if (row_w <= 0) {
      continue;
    }
    r[n * 4 + 0] = (int16_t)inset;
    r[n * 4 + 1] = (int16_t)y;
    r[n * 4 + 2] = (int16_t)row_w;
    r[n * 4 + 3] = 1;
    n++;
  }
  return n;
}

/* 把窗口裁成圆角胶囊：矩形以外不显示（露出壁纸），也不接收点击 */
static void apply_shape(void) {
  int16_t r[4 * 64];
  int n = round_rects(r, dock_w, DOCK_H, CORNER);
  x11_shape_rects(&c, dock, r, n);
}

/* 一个图标按钮（idx 从 0 开始，窗口内居中在它自己的格子里） */
static void button(int idx) {
  int x = PAD + idx * STEP;
  int y = (DOCK_H - ICON) / 2;
  int running = (idx >= N_LAUNCH);
  const x11_image_t* bm = running ? &icons[ICON_RUN] : &icons[idx];

  if (running) { /* 运行中的窗口在图标下方加一条高亮 */
    x11_set_gc(&c, gc, 0x0C, RUN_ON, RUN_ON);
    x11_fill_rect(&c, dock, gc, x + 4, DOCK_H - 9, ICON - 8, 3);
  }
  if (bm->px != NULL) {
    x11_put_image(&c, dock, gc, x + (ICON - bm->w) / 2,
                  y + (ICON - bm->h) / 2, bm->w, bm->h, bm->px);
    return;
  }
  /* 图标文件缺失时的回退：彩色方块 + 字母 */
  if (!running) {
    x11_set_gc(&c, gc, 0x0C, launch[idx].color, launch[idx].color);
    x11_fill_rect(&c, dock, gc, x, y, ICON, ICON);
    x11_set_gc(&c, gc, 0x0C, DOCK_FG, launch[idx].color);
    x11_image_text8(&c, dock, gc, x + ICON / 2 - 3, y + ICON / 2 + 6,
                    launch[idx].letter, 1);
  } else {
    x11_set_gc(&c, gc, 0x0C, X11_RGB(0x43, 0x4c, 0x5e), X11_RGB(0x43, 0x4c, 0x5e));
    x11_fill_rect(&c, dock, gc, x, y, ICON, ICON);
    x11_set_gc(&c, gc, 0x0C, DOCK_FG, X11_RGB(0x43, 0x4c, 0x5e));
    x11_image_text8(&c, dock, gc, x + ICON / 2 - 3, y + ICON / 2 + 6, "W", 1);
  }
}

static void draw(void);

/* 每次重画都留一行日志：dock 真出现"一直在刷新"时，看这行就知道是
 * Expose 洪峰、还是窗口列表在反复变（各一次/800ms），不用猜。 */
static void redraw(const char* why) {
  printf("dock: 重画（%s）运行窗口 %d 个\n", why, nruns);
  draw();
}

static void draw(void) {
  int i;
  int count = N_LAUNCH + nruns;
  int w = calc_w(count);

  if (w != dock_w) { /* 运行窗口增删：胶囊变宽/变窄，始终居中 */
    dock_w = w;
    x11_configure(&c, dock, (c.w - w) / 2, c.h - DOCK_H, w, DOCK_H);
    apply_shape(); /* 宽度变了，形状跟着重算 */
  }
  /* 整窗铺胶囊底色：四角已被 SHAPE 裁掉，所以边缘不会露黑块 */
  x11_set_gc(&c, gc, 0x0C, DOCK_BG2, DOCK_BG2);
  x11_fill_rect(&c, dock, gc, 0, 0, dock_w, DOCK_H);
  for (i = 0; i < N_LAUNCH; i++) {
    button(i);
  }
  for (i = 0; i < nruns; i++) {
    button(N_LAUNCH + i);
  }
}

/* 读 _NET_CLIENT_LIST，变化则返回 1。
 * 【读失败别当成"0 个窗口"】那会让胶囊先缩到最小、下一次读成功又长回来，
 * 看起来就是"dock 一直在刷新/跳"。失败一律保持现状。 */
static int refresh_runs(void) {
  uint32_t list[MAX_RUN];
  int n = x11_get_property32(&c, c.root, atom_client_list, list, MAX_RUN);
  int i, changed;
  if (n < 0) {
    return 0;
  }
  changed = (n != nruns);
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
  /* 发送给根窗口，data[0] = 目标窗口（与 wm.c 的约定一致）。
   * 【event-mask 必须是 Substructure*】SendEvent 的 event-mask 决定"投给哪些
   * 客户端"：服务器只把事件交给在目标窗口上选了 mask 里任一比特的客户端。
   * wm 在根窗口上选的是 SubstructureRedirect|SubstructureNotify(0x180000)，
   * 这里原先写的是 X11_EV_STRUCTURE(0x20000，表示根窗口自己的结构变化)，
   * 位对不上 ⇒ 服务器谁都不投，dock 点任务按钮完全没反应（最小化的窗口
   * 也永远恢复不了）。 */
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
  x11_send_event(&c, c.root,
                 X11_EV_SUBSTRUCTURE_REDIRECT | X11_EV_SUBSTRUCTURE_NOTIFY, ev);
}

static void on_button(const uint8_t* ev) {
  int x = (int16_t)(ev[24] | (ev[25] << 8));
  int rel = x - PAD;
  int idx;
  if (rel < 0) {
    return;
  }
  idx = rel / STEP;
  if (rel % STEP > ICON) { /* 点在间隙里 */
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
  int i, loaded = 0;
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  (void)argc;
  (void)argv;

  if (x11_connect(&c, NULL) < 0) {
    return 1;
  }
  /* 单独跑 dock（不起 wm）时也要有指针 */
  x11_install_arrow_cursor(&c);
  gc = x11_id(&c);
  font = x11_id(&c);
  if (x11_open_font(&c, font, "fixed") < 0) {
    fprintf(stderr, "dock: 打开 fixed 字体失败\n");
    return 1;
  }
  x11_create_gc(&c, gc, c.root);
  x11_set_gc_font(&c, gc, font);
  atom_client_list = x11_intern_atom(&c, X11_NET_CLIENT_LIST);
  atom_active = x11_intern_atom(&c, X11_NET_ACTIVE_WINDOW);

  for (i = 0; i < N_LAUNCH; i++) {
    if (x11_load_bmp(launch[i].icon, &icons[i]) == 0) {
      loaded++;
    }
  }
  if (x11_load_bmp("dock-win.bmp", &icons[ICON_RUN]) == 0) {
    loaded++;
  }

  refresh_runs();
  dock_w = calc_w(N_LAUNCH + nruns);
  dock = x11_create_window(&c, (c.w - dock_w) / 2, c.h - DOCK_H, dock_w, DOCK_H,
                           DOCK_BG2, 1, X11_EV_EXPOSURE | X11_EV_BUTTON_PRESS);
  if (dock == 0) {
    fprintf(stderr, "dock: 建窗口失败\n");
    return 1;
  }
  /* 把窗口裁成圆角胶囊：矩形以外的区域不显示（露出桌面壁纸），
   * 所以窗口不必铺满屏宽，边缘也不会有黑色矩形块。 */
  if (x11_shape_probe(&c) < 0) {
    fprintf(stderr, "dock: 没有 SHAPE 扩展，胶囊退化成直角矩形\n");
  }
  apply_shape(); /* 先裁形状再 map，避免映射瞬间闪一下方角 */
  if (x11_sync(&c) < 0) { /* 形状请求有错会在这里报出来（否则是静默方角） */
    fprintf(stderr, "dock: 圆角形状请求被服务器拒绝\n");
  }
  x11_map_window(&c, dock);
  x11_raise(&c, dock);
  draw();
  printf("dock: %dx%d 胶囊位于 (%d,%d)，%d 个启动器 + %d 个运行窗口，%d 个图标%s\n",
         dock_w, DOCK_H, (c.w - dock_w) / 2, c.h - DOCK_H, N_LAUNCH, nruns,
         loaded, (c.shape_op > 0) ? "（圆角）" : "");

  for (;;) {
    struct pollfd pfd;
    int pr, r;
    pfd.fd = c.fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    pr = poll(&pfd, 1, 800);
    if (pr > 0) {
      int exposed = 0;
      while ((r = x11_next_event(&c, ev)) == 1) {
        if (ev[0] == X11_EXPOSE) {
          /* 【一批 Expose 只重画一次】Expose 的 count 字段（offset 12）表示
           * "后面还有多少条 Expose 等着发"：映射/换形状时服务器一次能甩出
           * 十几条，逐条重画就是肉眼可见的闪。等到 count==0 再画。 */
          int more = (int)ev[12] | ((int)ev[13] << 8);
          if (x11_ev_u32(ev, 4) == dock && more == 0) {
            exposed = 1;
          }
        } else if (ev[0] == X11_BUTTON_PRESS && ev[1] == 1 /* 只有左键 */) {
          if (x11_ev_u32(ev, 12) == dock) {
            on_button(ev);
          }
        }
      }
      if (exposed) {
        redraw("Expose");
      }
      if (r < 0) { /* 连接断了（服务器退出/被杀）：继续跑只会空转 */
        fprintf(stderr, "dock: 与 X 的连接断开，退出\n");
        return 1;
      }
    } else if (pr == 0) {
      /* 【只在 poll 超时的时候查列表】早先是"每轮都查"（tick 清零后判断恒
       * 成立），事件一密就变成每个事件一次 GetProperty 往返；现在事件密集
       * 时不做多余的同步请求，空闲时每 0.8s 查一次。 */
      if (refresh_runs()) {
        redraw("窗口列表变化");
      }
    }
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
  }
  return 0;
}
