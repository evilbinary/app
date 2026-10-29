/* weston 端到端冒烟（阶段 2）：在本内核上把 libweston 拉起来。
 *
 * 结构：一个二进制 fork 出 server / client 两个进程。
 *   server：wl_display_create → weston_compositor_create（pixman 渲染器）
 *           → headless 后端（weston_backend_init）→ 创建并 enable 一个
 *           640x480 输出 → 监听 unix socket → wl_display_run
 *   client：连 socket，枚举 registry，检查核心 global 是否齐（wl_compositor /
 *           wl_subcompositor / wl_shm / wl_output / wp_viewporter /
 *           wp_presentation / zxdg_output_manager_v1），roundtrip 成功即算通。
 *
 * 这一步验证的是 weston 的事件循环（epoll + timerfd）、pixman 渲染器初始化、
 * headless 输出 enable、协议 global 注册。像素上屏（fbdev/DRM 后端）不在本阶段。
 *
 * 输出 LTP 风格 TPASS/TFAIL + Summary，tools/ltp_batch.py 直跑。
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland-server.h>

#include <libweston/backend-headless.h>
#include <libweston/libweston.h>
#include <libweston/windowed-output-api.h>
#include <xkbcommon/xkbcommon.h>
#include <mini-shell.h>

#define SOCK_NAME "wayland-weston-test"

/* 输出尺寸：实际按 /dev/fb0 的尺寸来（yy_fb_size），这样合成结果能 1:1
 * 搬到显存；没有 framebuffer 的机器回退 640x480（纯 headless 自测）。 */
static int g_out_w = 640;
static int g_out_h = 480;

/* 后端模块入口（libweston 正常走 dlopen；这里静态链接后直接调用） */
extern int weston_backend_init(struct weston_compositor *compositor,
                               struct weston_backend_config *config_base);

/* eggs/weston/compat/backend-access.c：取第一个已注册后端（公开头无迭代 API） */
extern struct weston_backend *
weston_compositor_get_first_backend(struct weston_compositor *compositor);

static int g_pass;
static int g_fail;

static void check(int cond, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#include <stdarg.h>
static void check(int cond, const char *fmt, ...) {
  va_list ap;
  char buf[512];

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (cond) {
    g_pass++;
    printf("TPASS: %s\n", buf);
  } else {
    g_fail++;
    printf("TFAIL: %s\n", buf);
  }
  fflush(stdout);
}

/* ===================== server ===================== */

/* 进度探针：串口输出在多进程早期可能丢，同步写文件最可靠 */
#define STEP_LOG "/tmp/wl-weston-child.log"

/* ready 管道只在 fork 模式下存在；server 单独跑时传 -1 */
static void ready_send(int fd, char c) {
  if (fd >= 0)
    write(fd, &c, 1);
}

static void step(const char *s) {
  int fd = open(STEP_LOG, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd >= 0) {
    write(fd, s, strlen(s));
    write(fd, "\n", 1);
    close(fd);
  }
  printf("SERVER: %s\n", s);
  fflush(stdout);
}

/* weston 日志直通 stdout（串口控制台） */
static void weston_log_print(const char *fmt, va_list ap) {
  vprintf(fmt, ap);
  fflush(stdout);
}

static int g_outputs_enabled;
static struct weston_output *g_output;

/* head 出现/变化时为其建输出并 enable（windowed 后端标准流程） */
static void on_heads_changed(struct wl_listener *listener, void *data) {
  struct weston_compositor *ec = data;
  const struct weston_windowed_output_api *api =
      weston_windowed_output_get_api(ec, WESTON_WINDOWED_OUTPUT_HEADLESS);
  struct weston_head *head = NULL;

  (void)listener;

  while ((head = weston_compositor_iterate_heads(ec, head)) != NULL) {
    struct weston_output *out;

    if (!weston_head_is_connected(head) || weston_head_is_enabled(head))
      continue;

    out = weston_compositor_create_output(ec, head, weston_head_get_name(head));
    if (out == NULL) {
      step("create output failed");
      continue;
    }

    /* 窗口式后端要求先给定 scale（headless 的 output_set_size 里有
     * assert(output->base.current_scale)），再设尺寸。 */
    weston_output_set_scale(out, 1);
    weston_output_set_transform(out, WL_OUTPUT_TRANSFORM_NORMAL);

    if (api != NULL && api->output_set_size(out, g_out_w, g_out_h) < 0) {
      step("output_set_size failed");
      continue;
    }

    if (weston_output_enable(out) < 0) {
      step("output enable failed");
      continue;
    }

    /* 注意：这里【不能】立刻 weston_output_set_ready()。它会马上 schedule
     * 一次 repaint，而此刻场景图里还没有任何 view，compositor 会撞上
     * "empty scene graph at repaint" 断言退出。ready 由 mini-shell 在第一个
     * surface 入层之后再置位。 */
    g_outputs_enabled++;
    g_output = out;
    step("output enabled");

    if (mini_shell_init(ec, out) < 0)
      step("mini_shell_init failed");
  }
}

static struct wl_listener heads_listener = {
    .notify = on_heads_changed,
};

static int run_server(int ready_fd) {
  struct wl_display *display;
  struct weston_log_context *log_ctx;
  struct weston_compositor *ec;
  struct weston_backend *backend;
  const struct weston_windowed_output_api *api;
  struct weston_headless_backend_config bcfg;
  int rc = 1;

  step("start");

  /* 输出尺寸跟显存走：mini-shell 会把合成结果整帧 blit 到 /dev/fb0，
   * 两边尺寸一致才能 1:1 上屏（没有 fb 时 yy_fb_size 回退 640x480）。 */
  yy_fb_size(&g_out_w, &g_out_h);
  printf("SERVER: 输出 %dx%d（跟随 /dev/fb0）\n", g_out_w, g_out_h);
  fflush(stdout);

  /* weston 16 的日志走 log context（compositor_create 要求非 NULL）；
   * 这里挂一个直通 stdout 的 handler，方便在串口上看 weston 自己的诊断。 */
  weston_log_set_handler(weston_log_print, weston_log_print);
  log_ctx = weston_log_ctx_create();
  if (log_ctx == NULL) {
    step("weston_log_ctx_create failed");
    ready_send(ready_fd, 'E');
    return 1;
  }

  /* wl_display_add_socket 要求 XDG_RUNTIME_DIR；本系统惯例是 /tmp */
  if (setenv("XDG_RUNTIME_DIR", "/tmp", 1) != 0)
    step("setenv failed");
  step("env ok");

  display = wl_display_create();
  if (display == NULL) {
    step("wl_display_create failed");
    ready_send(ready_fd, 'E');
    return 1;
  }
  step("wl_display_create ok");

  ec = weston_compositor_create(display, log_ctx, NULL, NULL);
  if (ec == NULL) {
    step("weston_compositor_create failed");
    ready_send(ready_fd, 'E');
    return 1;
  }
  step("compositor created");

  /* 建 XKB 上下文与 RMLVO 缺省值（evdev/pc105/us）。不做这步，后面
   * headless 的 fake seat 建键盘时会拿 NULL context 去编 keymap 而崩。 */
  if (weston_compositor_set_xkb_rule_names(ec, NULL) < 0) {
    step("set xkb rule names failed");
    ready_send(ready_fd, 'E');
    return 1;
  }
  step("xkb context ready");

  memset(&bcfg, 0, sizeof(bcfg));
  bcfg.base.struct_version = WESTON_HEADLESS_BACKEND_CONFIG_VERSION;
  bcfg.base.struct_size = sizeof(bcfg);
  bcfg.renderer = WESTON_RENDERER_PIXMAN;
  bcfg.fake_seat = true;
  bcfg.refresh = 60000; /* mHz → 60Hz */
  bcfg.output_fb_alpha_encoding = WESTON_OUTPUT_FB_ALPHA_PREMULT;

  if (weston_backend_init(ec, (struct weston_backend_config *)&bcfg) < 0) {
    step("headless backend init failed");
    ready_send(ready_fd, 'E');
    return 1;
  }
  step("headless backend up");

  /* 后端就绪后必须收尾：选定 presentation clock、装默认 color manager
   * （noop）。不做这步，后面建 output 会在 compositor->color_manager 空指针
   * 上调 vtable 而崩。 */
  if (weston_compositor_backends_loaded(ec) < 0) {
    step("backends_loaded failed");
    ready_send(ready_fd, 'E');
    return 1;
  }
  step("backends loaded");

  /* 输出不是直接 create 出来的：windowed 后端的标准做法是先建 head
   * （api->create_head），head 挂进 compositor 后触发 heads_changed 信号，
   * 在回调里为每个 head 建 output、设尺寸并 enable。 */
  weston_compositor_add_heads_changed_listener(ec, &heads_listener);

  api = weston_windowed_output_get_api(ec, WESTON_WINDOWED_OUTPUT_HEADLESS);
  if (api == NULL) {
    step("windowed output api missing");
    ready_send(ready_fd, 'E');
    return 1;
  }

  backend = weston_compositor_get_first_backend(ec);
  if (backend == NULL) {
    step("no backend registered");
    ready_send(ready_fd, 'E');
    return 1;
  }

  if (api->create_head(backend, "headless") < 0) {
    step("create_head failed");
    ready_send(ready_fd, 'E');
    return 1;
  }
  step("head created");

  /* 后端的 add_head 只是把 head 挂上并排队一次 heads_changed 通知（idle 里发），
   * 这里主动 flush 一次，立刻走我们的回调把输出建起来并 enable。 */
  weston_compositor_flush_heads_changed(ec);

  if (g_outputs_enabled == 0) {
    step("no output enabled");
    ready_send(ready_fd, 'E');
    return 1;
  }

  weston_compositor_wake(ec);

  if (wl_display_add_socket(display, SOCK_NAME) != 0) {
    step("add socket failed");
    ready_send(ready_fd, 'E');
    return 1;
  }
  step("socket listening");
  ready_send(ready_fd, 'R');

  wl_display_run(display);

  rc = 0;
  wl_display_destroy_clients(display);
  weston_compositor_destroy(ec);
  wl_display_destroy(display);
  return rc;
}

/* ===================== client ===================== */

struct globals {
  int compositor;
  int subcompositor;
  int shm;
  int output;
  int viewporter;
  int presentation;
  int xdg_output_manager;
  int seat;
  int total;

  /* 画像素要用到的对象 */
  struct wl_compositor *compositor_obj;
  struct wl_shm *shm_obj;
  struct wl_seat *seat_obj;
};

/* 定义在客户端段（app 之后），registry 回调里就要用，先声明 */
static const struct wl_seat_listener seat_listener;
static const struct wl_pointer_listener pointer_listener;

static void on_global(void *data, struct wl_registry *reg, uint32_t name,
                      const char *iface, uint32_t version) {
  struct globals *g = data;

  (void)version;
  g->total++;

  if (strcmp(iface, "wl_compositor") == 0) {
    g->compositor++;
    g->compositor_obj =
        wl_registry_bind(reg, name, &wl_compositor_interface, 4);
  } else if (strcmp(iface, "wl_subcompositor") == 0)
    g->subcompositor++;
  else if (strcmp(iface, "wl_shm") == 0) {
    g->shm++;
    g->shm_obj = wl_registry_bind(reg, name, &wl_shm_interface, 1);
  }
  else if (strcmp(iface, "wl_output") == 0)
    g->output++;
  else if (strcmp(iface, "wp_viewporter") == 0)
    g->viewporter++;
  else if (strcmp(iface, "wp_presentation") == 0)
    g->presentation++;
  else if (strcmp(iface, "zxdg_output_manager_v1") == 0)
    g->xdg_output_manager++;
  else if (strcmp(iface, "wl_seat") == 0) {
    g->seat++;
    /* 绑 version 1：只需要 capabilities，pointer 的 enter/leave/motion/
     * button/axis 就够拖动窗口，不用管 v5 的 frame/axis_source。 */
    g->seat_obj = wl_registry_bind(reg, name, &wl_seat_interface, 1);
    wl_seat_add_listener(g->seat_obj, &seat_listener, g);
  }
}

static const struct wl_registry_listener registry_listener = {
    .global = on_global,
};

/* ===================== 客户端：一个可以用鼠标拖动的窗口 =====================
 * 整块 surface 就是全屏画布，客户端自己画"桌面 + 窗口 + 鼠标指针"：
 *   · 鼠标事件来自 wl_seat/wl_pointer（server 侧 mini-shell 把
 *     /dev/input/event0 翻成 wl_pointer 的 enter/motion/button）；
 *   · 按住标题栏拖动 → 改窗口坐标 → 重绘 + commit；
 *   · 指针位置由客户端自己画（合成器没有硬件光标）。 */
#define WIN_W 620
#define WIN_H 400
#define TITLE_H 52 /* macOS 风格大标题栏 */
#define WIN_R 12   /* 窗口圆角半径 */
#define SB_W 190   /* 侧边栏宽度 */
#define BTN_LEFT_K 0x110

static struct {
  uint32_t *px;      /* 当前正在画的缓冲 */
  uint32_t *pxbuf[2];
  int w;
  int h;
  int stride;
  struct wl_surface *surf;
  struct wl_buffer *buf[2];
  int busy[2];       /* 1 = 合成器还占着这块 buffer，不能写 */
  int cur;           /* 下一帧用哪块 */
  struct wl_pointer *pointer;
  struct timespec touch; /* 最后一次交互，用于空闲退出 */
  double cx, cy; /* 指针在 surface 内的坐标 */
  int have_ptr;
  int dragging;
  int dragged;
  double drag_dx, drag_dy;
  double win_x, win_y;
  int dirty;
} app;

static void put_px(int x, int y, uint32_t c) {
  if (x < 0 || y < 0 || x >= app.w || y >= app.h)
    return;
  app.px[(size_t)y * app.w + x] = c;
}

/* 半透明叠加：用来做柔和投影（缓冲是 XRGB，没有真 alpha 通道，自己混） */
static void blend_px(int x, int y, uint32_t c, int a) {
  uint32_t d;
  int r, g, b;

  if (x < 0 || y < 0 || x >= app.w || y >= app.h)
    return;
  d = app.px[(size_t)y * app.w + x];
  r = (int)(((c >> 16) & 0xff) * a + ((d >> 16) & 0xff) * (255 - a)) / 255;
  g = (int)(((c >> 8) & 0xff) * a + ((d >> 8) & 0xff) * (255 - a)) / 255;
  b = (int)((c & 0xff) * a + (d & 0xff) * (255 - a)) / 255;
  app.px[(size_t)y * app.w + x] = 0xff000000u | ((uint32_t)r << 16) |
                                  ((uint32_t)g << 8) | (uint32_t)b;
}

static void fill_rect(int x, int y, int w, int h, uint32_t c) {
  int i, j;

  for (j = 0; j < h; j++)
    for (i = 0; i < w; i++)
      put_px(x + i, y + j, c);
}

/* 把颜色往白色方向提亮 pct% */
static uint32_t lighten(uint32_t c, int pct) {
  int r = (int)((c >> 16) & 0xff);
  int g = (int)((c >> 8) & 0xff);
  int b = (int)(c & 0xff);

  r += (255 - r) * pct / 100;
  g += (255 - g) * pct / 100;
  b += (255 - b) * pct / 100;
  return 0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void fill_circle(int cx, int cy, int r, uint32_t c) {
  int y, x;

  for (y = -r; y <= r; y++)
    for (x = -r; x <= r; x++)
      if (x * x + y * y <= r * r)
        put_px(cx + x, cy + y, c);
}

/* 圆角矩形第 row 行的水平范围（inset 按圆弧算，纯整数） */
static void round_span(int row, int x, int y, int w, int h, int r, int *x0,
                       int *x1) {
  int dy = 0;
  int inset = 0;

  if (row < y || row >= y + h) {
    *x0 = 1;
    *x1 = 0;
    return;
  }
  if (row < y + r)
    dy = (y + r) - row;
  else if (row >= y + h - r)
    dy = row - (y + h - r - 1);

  while (inset < r && (r - inset) * (r - inset) + dy * dy > r * r)
    inset++;

  *x0 = x + inset;
  *x1 = x + w - 1 - inset;
}

static void fill_round(int x, int y, int w, int h, int r, uint32_t c) {
  int row;

  for (row = y; row < y + h; row++) {
    int x0, x1;

    round_span(row, x, y, w, h, r, &x0, &x1);
    if (x0 <= x1)
      fill_rect(x0, row, x1 - x0 + 1, 1, c);
  }
}

/* 圆角矩形右/左各留 1px 的描边：先填边框色，再内缩 1px 填内部色 */
static void fill_round_box(int x, int y, int w, int h, int r, uint32_t border,
                           uint32_t fill) {
  fill_round(x, y, w, h, r, border);
  fill_round(x + 1, y + 1, w - 2, h - 2, r > 1 ? r - 1 : 1, fill);
}

/* 桌面壁纸：左上偏蓝、右下偏紫的对角渐变（macOS 深色壁纸的观感） */
static void draw_desktop(void) {
  int x, y;

  for (y = 0; y < app.h; y++) {
    for (x = 0; x < app.w; x++) {
      const int t = (app.w + app.h) > 0
                        ? (x * 255 / app.w + y * 255 / app.h) / 2
                        : 0;
      const int r = 28 + t * 30 / 255;  /* 28 → 58 */
      const int g = 36 + t * 12 / 255;  /* 36 → 48 */
      const int b = 64 + t * 28 / 255;  /* 64 → 92 */

      app.px[(size_t)y * app.w + x] = 0xff000000u | ((uint32_t)r << 16) |
                                      ((uint32_t)g << 8) | (uint32_t)b;
    }
  }
}

/* 5x7 点阵只做标题需要的几个字母（每字符 5 列，bit0 是最上一行） */
static const unsigned char glyph_F[5] = {0x7f, 0x01, 0x09, 0x09, 0x09};
static const unsigned char glyph_i[5] = {0x40, 0x44, 0x7d, 0x40, 0x00};
static const unsigned char glyph_n[5] = {0x7c, 0x04, 0x04, 0x7c, 0x00};
static const unsigned char glyph_d[5] = {0x38, 0x44, 0x44, 0x44, 0x7f};
static const unsigned char glyph_e[5] = {0x38, 0x54, 0x54, 0x54, 0x18};
static const unsigned char glyph_r[5] = {0x7c, 0x0c, 0x04, 0x0c, 0x08};

static const unsigned char *title_glyph(char c) {
  switch (c) {
    case 'F':
      return glyph_F;
    case 'i':
      return glyph_i;
    case 'n':
      return glyph_n;
    case 'd':
      return glyph_d;
    case 'e':
      return glyph_e;
    case 'r':
      return glyph_r;
    default:
      return NULL;
  }
}

static void draw_title_text(const char *s, int x, int y, int scale, uint32_t c) {
  for (; *s != '\0'; s++) {
    const unsigned char *g = title_glyph(*s);
    int col, row;

    if (g == NULL) {
      x += 4 * scale;
      continue;
    }
    for (col = 0; col < 5; col++)
      for (row = 0; row < 7; row++)
        if ((g[col] >> row) & 1)
          fill_rect(x + col * scale, y + row * scale, scale, scale, c);
    x += 6 * scale;
  }
}

/* 窗口内部的一行：自动被窗口圆角裁掉两侧（侧栏/状态栏贴边时用） */
static void fill_win_row(int row, int x, int w, uint32_t c) {
  int x0, x1;

  round_span(row, (int)app.win_x + 1, (int)app.win_y + 1, WIN_W - 2, WIN_H - 2,
             WIN_R - 1, &x0, &x1);
  if (x < x0) {
    w -= (x0 - x);
    x = x0;
  }
  if (x + w - 1 > x1)
    w = x1 - x + 1;
  if (w > 0)
    fill_rect(x, row, w, 1, c);
}

/* 柔和投影：由外向内几层半透明黑，只画窗口外圈（画满整块太慢）。
 * macOS 的窗口投影很大很淡，靠"层多 + alpha 小"来近似。 */
static void draw_shadow(void) {
  const int wx = (int)app.win_x;
  const int wy = (int)app.win_y;
  const int base = app.dragging ? 5 : 0;
  int layer;

  for (layer = 0; layer < 5; layer++) {
    const int grow = 24 - layer * 5;
    const int alpha = 4 + layer * 3 + base;
    const int sx = wx - grow;
    const int sy = wy - grow + 16;
    const int sw = WIN_W + grow * 2;
    const int sh = WIN_H + grow * 2;
    int row;

    for (row = sy; row < sy + sh; row++) {
      int x0, x1, x;

      round_span(row, sx, sy, sw, sh, WIN_R + grow, &x0, &x1);
      for (x = x0; x <= x1; x++) {
        /* 窗口本体所在区域不重复叠阴影 */
        if (row >= wy && row < wy + WIN_H && x >= wx && x < wx + WIN_W)
          continue;
        blend_px(x, row, 0x000000u, alpha);
      }
    }
  }
}

/* 侧栏（macOS 浅灰源列表）+ 右侧文件图标网格 */
static void draw_window_body(void) {
  const int wx = (int)app.win_x;
  const int wy = (int)app.win_y;
  const int top = wy + TITLE_H;
  const uint32_t sym[5] = {0xff007affu, 0xff34c759u, 0xffff9500u, 0xffaf52deu,
                           0xff5ac8fau};
  const uint32_t ico[6] = {0xff4c9be8u, 0xffe5c05au, 0xff8ec07cu,
                           0xffc07bd8u, 0xffe0705fu, 0xff54c7d0u};
  int i;
  int row;

  /* 侧栏底色（跟随窗口底部圆角） */
  for (row = top; row < wy + WIN_H - 1; row++)
    fill_win_row(row, wx + 1, SB_W, 0xfff5f5f7u);
  for (row = top; row < wy + WIN_H - 1; row++)
    fill_win_row(row, wx + 1 + SB_W, 1, 0xffe6e6eau);

  /* 侧栏 5 条：第一条是"选中态"（浅灰圆角块） */
  {
    const int label_w[5] = {92, 74, 86, 66, 80};

    for (i = 0; i < 5; i++) {
      const int iy = top + 14 + i * 36;
      const int label = i == 0 ? 0xffb4b4bcu : 0xffc6c6ccu;

      if (i == 0)
        fill_round(wx + 10, iy, SB_W - 20, 30, 7, 0xffe6e6eau);
      fill_round(wx + 20, iy + 6, 18, 18, 5, sym[i]);
      fill_round(wx + 48, iy + 11, label_w[i], 7, 3, label);
    }
  }

  /* 文件图标网格：3 列 × 2 行 */
  {
    const int name_w[6] = {46, 40, 50, 44, 38, 48};

    for (i = 0; i < 6; i++) {
      const int gx = wx + 1 + SB_W + 58 + (i % 3) * 124;
      const int gy = top + 62 + (i / 3) * 122;

      /* 图标：基色 + 上半部提亮（做出"上亮下暗"的层次，纯色块太假） */
      fill_round(gx, gy, 64, 64, 14, ico[i]);
      fill_round(gx + 3, gy + 3, 58, 30, 11, lighten(ico[i], 16));
      fill_round(gx + (64 - name_w[i]) / 2, gy + 78, name_w[i], 6, 3,
                 0xffbfbfc6u);
      fill_round(gx + 17, gy + 90, 30, 5, 2, 0xffdcdce1u);
    }
  }
}

static void draw_window(void) {
  const int wx = (int)app.win_x;
  const int wy = (int)app.win_y;

  draw_shadow();

  /* 窗体：极浅 1px 描边 + 纯白底 */
  fill_round_box(wx, wy, WIN_W, WIN_H, WIN_R, 0xffd4d4d8u, 0xffffffffu);

  draw_window_body();

  /* 标题栏：macOS 风格三个圆点 + 居中标题 */
  fill_circle(wx + 20, wy + TITLE_H / 2, 6, 0xffff5f57u);
  fill_circle(wx + 40, wy + TITLE_H / 2, 6, 0xfffebc2eu);
  fill_circle(wx + 60, wy + TITLE_H / 2, 6, 0xff28c840u);

  draw_title_text("Finder", wx + WIN_W / 2 - 29, wy + (TITLE_H - 14) / 2, 2,
                  0xff1d1d1fu);
}

/* 鼠标指针：白色箭头 + 深色描边（和常见光标一致） */
static void draw_cursor(void) {
  static const int arw[17] = {1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 5, 3, 3, 3};
  const int x = (int)app.cx;
  const int y = (int)app.cy;
  int i, j;

  if (!app.have_ptr)
    return;
  for (i = 0; i < 17; i++)
    for (j = 0; j < arw[i]; j++)
      blend_px(x + j + 2, y + i + 2, 0x000000u, 90);
  for (i = 0; i < 17; i++)
    for (j = 0; j < arw[i]; j++)
      put_px(x + j, y + i, 0xffffffffu);
  /* 描边 */
  for (i = 0; i < 17; i++) {
    put_px(x - 1, y + i, 0xff2b3038u);
    put_px(x + arw[i], y + i, 0xff2b3038u);
  }
  for (j = 0; j < 7; j++)
    put_px(x + j, y - 1, 0xff2b3038u);
  for (j = 0; j < 3; j++)
    put_px(x + j, y + 17, 0xff2b3038u);
}

static void render_app(void);

/* 画好一帧并提交：先等这块 buffer 被合成器释放（release 事件），再往里画，
 * 否则合成器会在"画到一半的缓冲"上采样，屏幕上就是半截窗口。 */
static int commit_frame(struct wl_display *display) {
  int idx = app.cur;
  int k;

  for (k = 0; k < 20 && app.busy[idx]; k++) {
    if (wl_display_roundtrip(display) < 0)
      return -1;
  }
  if (app.busy[idx])
    return -1; /* 合成器还没放出这块，本帧跳过（下一轮再试） */

  app.px = app.pxbuf[idx];
  render_app();
  wl_surface_attach(app.surf, app.buf[idx], 0, 0);
  wl_surface_damage(app.surf, 0, 0, app.w, app.h);
  wl_surface_commit(app.surf);
  app.busy[idx] = 1;
  app.cur = idx ^ 1;
  wl_display_flush(display);
  return 0;
}

static void render_app(void) {
  draw_desktop();
  draw_window();
  draw_cursor();
  app.dirty = 0;
}

/* ---- wl_seat / wl_pointer ---- */

static void app_touch(void) { clock_gettime(CLOCK_MONOTONIC, &app.touch); }

static void on_seat_capabilities(void *data, struct wl_seat *seat,
                                 uint32_t caps) {
  (void)data;
  if ((caps & WL_SEAT_CAPABILITY_POINTER) && app.pointer == NULL) {
    app.pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(app.pointer, &pointer_listener, NULL);
    printf("APP: seat 提供 pointer\n");
  }
}

static void on_seat_name(void *data, struct wl_seat *seat, const char *name) {
  (void)data;
  (void)seat;
  printf("APP: seat 名字 %s\n", name);
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = on_seat_capabilities,
    .name = on_seat_name,
};

static void on_ptr_enter(void *d, struct wl_pointer *p, uint32_t serial,
                         struct wl_surface *surf, wl_fixed_t sx, wl_fixed_t sy) {
  (void)d;
  (void)p;
  (void)serial;
  (void)surf;
  app.cx = wl_fixed_to_double(sx);
  app.cy = wl_fixed_to_double(sy);
  app.have_ptr = 1;
  app.dirty = 1;
  app_touch();
}

static void on_ptr_leave(void *d, struct wl_pointer *p, uint32_t serial,
                         struct wl_surface *surf) {
  (void)d;
  (void)p;
  (void)serial;
  (void)surf;
  /* 只隐藏光标，不打断拖动：合成器在某些时机（隐式抓取切换）会补发
   * leave，把 dragging 清掉会让窗口"拖到一半松手" */
  app.have_ptr = 0;
  app.dirty = 1;
}

static void on_ptr_motion(void *d, struct wl_pointer *p, uint32_t time,
                          wl_fixed_t sx, wl_fixed_t sy) {
  (void)d;
  (void)p;
  (void)time;
  app.cx = wl_fixed_to_double(sx);
  app.cy = wl_fixed_to_double(sy);
  app.have_ptr = 1;

  if (app.dragging) {
    app.win_x = app.cx - app.drag_dx;
    app.win_y = app.cy - app.drag_dy;
    /* 别把窗口拖到屏幕外：至少留 80x40 在里面 */
    if (app.win_x < -(WIN_W - 80))
      app.win_x = -(WIN_W - 80);
    if (app.win_x > app.w - 80)
      app.win_x = app.w - 80;
    if (app.win_y < 0)
      app.win_y = 0;
    if (app.win_y > app.h - 40)
      app.win_y = app.h - 40;
    if (!app.dragged) {
      app.dragged = 1;
      printf("APP: 窗口跟手移动 → (%.0f,%.0f)\n", app.win_x, app.win_y);
    }
  }
  app.dirty = 1;
  app_touch();
}

static void on_ptr_button(void *d, struct wl_pointer *p, uint32_t serial,
                          uint32_t time, uint32_t button, uint32_t state) {
  const int wx = (int)app.win_x;
  const int wy = (int)app.win_y;

  (void)d;
  (void)p;
  (void)serial;
  (void)time;

  if (button != BTN_LEFT_K)
    return;

  app_touch();

  if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
    if (app.cx >= wx && app.cx < wx + WIN_W && app.cy >= wy &&
        app.cy < wy + TITLE_H) {
      app.dragging = 1;
      app.dragged = 0;
      app.drag_dx = app.cx - app.win_x;
      app.drag_dy = app.cy - app.win_y;
      app.dirty = 1;
      printf("APP: 抓住标题栏，开始拖动\n");
    }
  } else if (app.dragging) {
    app.dragging = 0;
    app.dirty = 1;
    printf("APP: 放下窗口 → (%d,%d)\n", (int)app.win_x, (int)app.win_y);
  }
}

static void on_ptr_axis(void *d, struct wl_pointer *p, uint32_t time,
                        uint32_t axis, wl_fixed_t value) {
  (void)d;
  (void)p;
  (void)time;
  (void)axis;
  (void)value;
}

static const struct wl_pointer_listener pointer_listener = {
    .enter = on_ptr_enter,
    .leave = on_ptr_leave,
    .motion = on_ptr_motion,
    .button = on_ptr_button,
    .axis = on_ptr_axis,
};

/* 合成器渲染完一帧才发 release；同一块 buffer 在此之前不能再写 */
static void on_buffer_release(void *data, struct wl_buffer *buffer) {
  (void)buffer;
  *(int *)data = 0;
}

static const struct wl_buffer_listener buffer_listener = {
    .release = on_buffer_release,
};

static int run_client(void) {
  struct wl_display *display;
  struct wl_registry *registry;
  struct globals g;
  int i;

  step("client start");

  /* server 可能还在起，重试几次 */
  for (i = 0; i < 50; i++) {
    display = wl_display_connect(SOCK_NAME);
    if (display != NULL)
      break;
    usleep(100 * 1000);
  }
  if (display == NULL) {
    step("client connect failed");
    g_fail++;
    return 1;
  }
  step("client connected");

  memset(&g, 0, sizeof(g));
  registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, &g);

  step("client roundtrip...");
  if (wl_display_roundtrip(display) < 0) {
    step("client roundtrip failed");
    check(0, "wl_display_roundtrip 返回 %d", wl_display_get_error(display));
    return 1;
  }
  step("client roundtrip done");
  check(1, "wl_display_roundtrip 完成（server 事件循环存活）");

  check(g.compositor == 1, "wl_compositor global 已注册");
  check(g.subcompositor == 1, "wl_subcompositor global 已注册");
  check(g.shm == 1, "wl_shm global 已注册");
  check(g.output >= 1, "wl_output global 已注册（headless 输出 enable 成功）");
  check(g.viewporter == 1, "wp_viewporter global 已注册");
  check(g.presentation == 1, "wp_presentation global 已注册");
  check(g.xdg_output_manager == 1, "zxdg_output_manager_v1 global 已注册");
  check(g.total >= 7, "registry 共 %d 个 global", g.total);

  /* ---- 全屏画布：可拖动窗口 ---- *
   * 画布尺寸跟随 /dev/fb0（与 server 的 output 一致 ⇒ 1:1 上屏）。
   * 首帧交给 server 抓帧自检；之后进事件循环，按住标题栏拖动即可移动窗口。 */
  {
    void *base;
    struct wl_shm_pool *pool;
    int fd;

    yy_fb_size(&app.w, &app.h);
    app.stride = app.w * 4;
    check(app.w > 0 && app.h > 0, "客户端画布 %dx%d（跟随 /dev/fb0）", app.w,
          app.h);

    fd = memfd_create("wl-weston-shm", 0);
    check(fd >= 0, "memfd_create 返回 fd=%d", fd);
    if (fd < 0) {
      wl_display_disconnect(display);
      return 1;
    }

    check(ftruncate(fd, (off_t)app.stride * app.h * 2) == 0,
          "ftruncate(%d) 成功（%d 字节 x2）", fd, app.stride * app.h);

    base = mmap(NULL, (size_t)app.stride * app.h * 2, PROT_READ | PROT_WRITE,
                MAP_SHARED, fd, 0);
    check(base != MAP_FAILED, "mmap 共享页 %d 字节 x2", app.stride * app.h);
    if (base == MAP_FAILED) {
      close(fd);
      wl_display_disconnect(display);
      return 1;
    }
    app.pxbuf[0] = base;
    app.pxbuf[1] = (uint32_t *)((char *)base + (size_t)app.stride * app.h);
    app.px = app.pxbuf[0];

    /* 窗口摆中间，指针先放在窗口标题栏上 */
    app.win_x = (app.w - WIN_W) / 2.0;
    app.win_y = (app.h - WIN_H) / 2.0;
    app.cx = app.win_x + WIN_W / 2.0;
    app.cy = app.win_y + TITLE_H / 2.0;

    app.surf = wl_compositor_create_surface(g.compositor_obj);
    pool = wl_shm_create_pool(g.shm_obj, fd, app.stride * app.h * 2);
    app.buf[0] = wl_shm_pool_create_buffer(pool, 0, app.w, app.h, app.stride,
                                           WL_SHM_FORMAT_XRGB8888);
    app.buf[1] = wl_shm_pool_create_buffer(pool, app.stride * app.h, app.w,
                                           app.h, app.stride,
                                           WL_SHM_FORMAT_XRGB8888);
    check(app.surf != NULL && pool != NULL && app.buf[0] != NULL &&
              app.buf[1] != NULL,
          "surface/pool/buffer 创建成功（双缓冲）");

    if (app.surf != NULL && app.buf[0] != NULL && app.buf[1] != NULL) {
      struct timespec t_last;
      struct timespec t_now;
      int frames = 0;

      wl_buffer_add_listener(app.buf[0], &buffer_listener, &app.busy[0]);
      wl_buffer_add_listener(app.buf[1], &buffer_listener, &app.busy[1]);

      if (commit_frame(display) < 0)
        check(0, "首帧 commit 失败");
      else
        check(1, "首帧已提交（双缓冲 + 等 release）");

      if (wl_display_roundtrip(display) < 0)
        check(0, "commit 后 roundtrip 失败");
      else
        check(1, "commit 后 roundtrip 成功（server 已消费请求）");

      check(app.pointer != NULL,
            "wl_seat 已提供 pointer（鼠标可以拖动窗口）");

      /* 事件循环：每轮 roundtrip 把 server 发来的一批事件收干净（本内核的
       * poll 在 unix socket 上是"存在即就绪"的简化实现，roundtrip 更可靠）。
       * 只有状态变化才重绘，且两次重绘之间至少间隔一帧（~35ms）：合成器
       * 每 50ms 整屏重绘一次，画得太勤会一直在"画到一半的缓冲"上采样。 */
      clock_gettime(CLOCK_MONOTONIC, &t_last);
      app_touch();
      for (;;) {
        long idle_ms;
        long since_render;

        if (wl_display_roundtrip(display) < 0) {
          check(0, "事件循环 roundtrip 失败（server 退出？）");
          break;
        }

        clock_gettime(CLOCK_MONOTONIC, &t_now);
        since_render = (t_now.tv_sec - t_last.tv_sec) * 1000L +
                       (t_now.tv_nsec - t_last.tv_nsec) / 1000000L;

        if (app.dirty && since_render >= 35 && commit_frame(display) == 0) {
          frames++;
          t_last = t_now;
        }

        idle_ms = (t_now.tv_sec - app.touch.tv_sec) * 1000L +
                  (t_now.tv_nsec - app.touch.tv_nsec) / 1000000L;
        if (idle_ms > 15L * 60L * 1000L) {
          printf("APP: 15 分钟没有交互，退出（画面留在屏幕上）\n");
          break;
        }
        usleep(10 * 1000);
      }

      check(1, "客户端事件循环结束：重绘 %d 帧，拖动过窗口=%s", frames,
            app.dragged ? "是" : "否");
    }
  }

  wl_registry_destroy(registry);
  wl_display_disconnect(display);
  return 0;
}

/* ===================== xkb 自检 ===================== */

static int run_xkb(void) {
  struct xkb_context *ctx;
  struct xkb_keymap *map;
  struct xkb_rule_names names = {0};

  names.rules = "evdev";
  names.model = "pc105";
  names.layout = "us";
  names.variant = "";
  names.options = "";

  ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  check(ctx != NULL, "xkb_context_new() 返回 %p", (void *)ctx);
  if (ctx == NULL) {
    ctx = xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES);
    check(ctx != NULL, "xkb_context_new(NO_DEFAULT_INCLUDES) 返回 %p", (void *)ctx);
  }
  if (ctx == NULL)
    return 1;

  map = xkb_keymap_new_from_names(ctx, &names, 0);
  check(map != NULL, "xkb_keymap_new_from_names(evdev/pc105/us) 返回 %p",
        (void *)map);

  if (map == NULL) {
    /* 没有 xkeyboard-config 数据时，退回内嵌的最小 keymap 文本 */
    static const char minimal[] =
        "xkb_keymap {\n"
        "  xkb_keycodes \"(evdev)\" { <ESC> = 9; <AE01> = 10; <AD01> = 24;"
        " <AC01> = 38; <AB01> = 52; <TLDE> = 49; <BKSP> = 22; <TAB> = 23;"
        " <RTRN> = 36; <LFSH> = 50; <SPCE> = 65; };\n"
        "  xkb_types \"(default)\" { include \"basic\" };\n"
        "  xkb_compatibility \"(default)\" { include \"basic\" };\n"
        "  xkb_symbols \"(default)\" {\n"
        "    key <TLDE> { [ grave, asciitilde ] };\n"
        "    key <AE01> { [ 1, exclam ] };\n"
        "    key <AD01> { [ q, Q ] };\n"
        "    key <AC01> { [ a, A ] };\n"
        "    key <AB01> { [ z, Z ] };\n"
        "    key <SPCE> { [ space ] };\n"
        "    key <RTRN> { [ Return ] };\n"
        "    key <BKSP> { [ BackSpace ] };\n"
        "    key <TAB> { [ Tab ] };\n"
        "    key <ESC> { [ Escape ] };\n"
        "    key <LFSH> { [ Shift_L ] };\n"
        "  };\n"
        "};\n";

    map = xkb_keymap_new_from_string(ctx, minimal,
                                     XKB_KEYMAP_FORMAT_TEXT_V1,
                                     XKB_KEYMAP_COMPILE_NO_FLAGS);
    check(map != NULL, "内嵌最小 keymap 编译 %p", (void *)map);
  }

  if (map != NULL) {
    struct xkb_state *st = xkb_state_new(map);
    check(st != NULL, "xkb_state_new 成功");
    if (st != NULL) {
      xkb_state_update_key(st, 38 /* <AC01>=a */, XKB_KEY_DOWN);
      check(1, "xkb_state_update_key 不崩");
      xkb_state_unref(st);
    }
    xkb_keymap_unref(map);
  }

  xkb_context_unref(ctx);
  printf("\nSummary:\npassed %d\nfailed %d\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}

/* ===================== main ===================== */

int main(int argc, char **argv) {
  int fds[2];
  pid_t pid;
  int server_rc;
  int client_rc;
  int st;

  setbuf(stdout, NULL);

  /* wayland 的 socket 目录约定：/tmp（本系统没有桌面会话环境） */
  setenv("XDG_RUNTIME_DIR", "/tmp", 1);

  /* 两种跑法：
   *   wl-weston             —— fork 出 server + client（默认，便于一条命令自测）
   *   wl-weston server      —— 只跑 server（前台阻塞，便于 shell 里 & 起）
   *   wl-weston client      —— 只跑 client（连已经起来的 server）
   * 拆分模式用于排查：本内核 fork 大二进制时子进程存活有问题。 */
  if (argc > 1 && strcmp(argv[1], "server") == 0)
    return run_server(-1);
  if (argc > 1 && strcmp(argv[1], "client") == 0)
    return run_client();
  if (argc > 1 && strcmp(argv[1], "xkb") == 0)
    return run_xkb();

  /* 后台常驻：父进程立刻返回（shell 拿到提示符），子进程跑 server。
   * 用于"一条命令起 server、下一条命令跑 client"的分步验证。 */
  if (argc > 1 && strcmp(argv[1], "daemon") == 0) {
    pid_t d = fork();
    if (d < 0)
      return 1;
    if (d > 0) {
      printf("DAEMON: pid=%d\n", (int)d);
      return 0;
    }
    setsid();
    return run_server(-1);
  }

  if (pipe(fds) != 0) {
    perror("pipe");
    return 1;
  }

  pid = fork();
  if (pid < 0) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    /* 子进程：跑 server，结束就 _exit（不要回到父进程流程） */
    close(fds[0]);
    run_server(fds[1]);
    _exit(0);
  }

  /* 父进程：不再等 pipe（阻塞读在这个内核上表现不可靠），
   * 直接用 run_client() 里的连接重试来等 server listen。 */
  close(fds[1]);
  close(fds[0]);

  client_rc = run_client();

  if (client_rc != 0) {
    /* client 失败时把 server 子进程的死法说清楚（早退/被信号/还活着） */
    int wr = waitpid(pid, &st, WNOHANG);

    if (wr == pid) {
      if (WIFEXITED(st))
        printf("TINFO: server 子进程已退出，exit code=%d\n", WEXITSTATUS(st));
      else if (WIFSIGNALED(st))
        printf("TINFO: server 子进程被信号 %d 杀死\n", WTERMSIG(st));
    } else {
      printf("TINFO: server 子进程仍在运行（client 连不上是 socket 层问题）\n");
    }
  }

  /* 收尾：先 SIGTERM（验证内核"默认动作=终止"是否生效），200ms 内没退出
   * 再补 SIGKILL 兜底（否则会永久卡在 waitpid）。 */
  {
    int waited = 0;
    int k;

    kill(pid, SIGTERM);
    for (k = 0; k < 20; k++) {
      if (waitpid(pid, &st, WNOHANG) == pid) {
        waited = 1;
        break;
      }
      usleep(10 * 1000);
    }
    if (!waited) {
      kill(pid, SIGKILL);
      waitpid(pid, &st, 0);
      printf("TINFO: SIGTERM 未终止 server，已用 SIGKILL 兜底\n");
    }
  }
  server_rc = WIFEXITED(st) ? WEXITSTATUS(st) : 1;

  check(client_rc == 0, "client 阶段全部检查完成");
  check(server_rc == 0 || WIFSIGNALED(st), "server 正常退出（rc=%d sig=%d）",
        server_rc, WIFSIGNALED(st) ? WTERMSIG(st) : 0);

  printf("\nSummary:\npassed %d\nfailed %d\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
