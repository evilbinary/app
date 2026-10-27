/*******************************************************************
 * xterm — 自研 X11 终端模拟器（替代 Xorg 的 xterm/lxterminal）。
 *
 * 三块结构：
 *   1) VT/ANSI 解析器：把 /dev/ptm 读到的字节流解析成字符网格操作。
 *      覆盖 shell 和日常命令需要的子集：CUP/CUU-CUD-CUF-CUB/ED/EL/ECH/
 *      SGR(16 色 + 256 色近似)/DECSTBM 滚动区/光标显隐/保存恢复光标，
 *      OSC 标题串吞掉不画。全屏程序（vim/less 的屏幕重画）不支持。
 *   2) 渲染：字符网格逐行按 (fg,bg) 相同的连续段分组发 ImageText8
 *      （fixed 6x13），ImageText8 自带背景着色所以每行只需一条底色
 *      FillRect；只重绘脏行，空闲零请求。光标 = 单元格实心块反色字。
 *   3) 会话：open("/dev/ptm") 拿主端 → ioctl(IOC_SLAVE) 取从端号 →
 *      fork 子进程把 /dev/pts/N dup2 到 0/1/2 跑 /bin/sh。
 *
 * 【内核配套】pty 从端空读回 -EAGAIN，sys_read 对 V_BLOCK_READ 节点走
 * sys_rw_wait 挂起、写入方 thread_wait_notify 唤醒 —— 所以子进程可以
 * 安心地阻塞 read，xterm 主端则恒非阻塞（本文件轮询排空）。
 *
 * 主循环 poll(X fd, 25ms)：每轮排空主端缓冲喂解析器、处理 X 事件、
 * 收尸（shell 退出则关窗）。键盘事件先取一次 keycode→keysym 表
 * （GetKeyboardMapping），按键时按 shift 选列、ctrl 折算控制码、
 * 方向键/功能键翻成转义序列，写回主端。
 *
 * 用法：xterm [WxH[+X+Y]]（按像素窗口尺寸反推字符格，默认 80x24）
 ********************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "x11.h"

/* 内核 pty 的私有 ioctl（同 duck/modules/pty/pty.h，用户态拿不到内核头） */
#define IOC_PTY_MAGIC 'p'
#define IOC_SLAVE _IOW(IOC_PTY_MAGIC, 1, int)
#define IOC_MASTER_READ_NOBLOCK _IOW(IOC_PTY_MAGIC, 5, int)

/* fixed 是 6x13 点阵（Xorg 内置，panel/dock/xcalc 同一假设） */
#define CH_W 6
#define CH_H 13
#define BASELINE 11 /* 基线 = 单元格顶 + ascent(11) */
#define PAD 4
#define MIN_COLS 20
#define MIN_ROWS 4
#define DEF_COLS 80
#define DEF_ROWS 24

#define COL_DEFAULT 0xff /* 颜色下标哨兵 = 默认前景/背景 */

#define BG_DEFAULT X11_RGB(0x1b, 0x1e, 0x23)
#define FG_DEFAULT X11_RGB(0xd8, 0xde, 0xe9)
#define CURSOR_COLOR X11_RGB(0x88, 0xc0, 0xd0)

/* SGR 调色板（nord 系）：0-7 普通、8-15 亮色 */
static const uint32_t palette[16] = {
    X11_RGB(0x3b, 0x42, 0x52), X11_RGB(0xbf, 0x61, 0x6a),
    X11_RGB(0xa3, 0xbe, 0x8c), X11_RGB(0xeb, 0xcb, 0x8b),
    X11_RGB(0x81, 0xa1, 0xc1), X11_RGB(0xb4, 0x8e, 0xad),
    X11_RGB(0x88, 0xc0, 0xd0), X11_RGB(0xe5, 0xe9, 0xf0),
    X11_RGB(0x4c, 0x56, 0x6a), X11_RGB(0xd0, 0x6c, 0x78),
    X11_RGB(0xb1, 0xd9, 0x9c), X11_RGB(0xf2, 0xd9, 0x9c),
    X11_RGB(0x9c, 0xb8, 0xd8), X11_RGB(0xc9, 0x9f, 0xc3),
    X11_RGB(0x9f, 0xd3, 0xdd), X11_RGB(0xec, 0xef, 0xf4),
};

typedef struct {
  uint8_t ch;
  uint8_t fg, bg;
} cell_t;

static cell_t* grid;
static int cols = DEF_COLS, rows = DEF_ROWS;
static int cx, cy;
static int cur_visible = 1;
static uint8_t cur_fg = COL_DEFAULT, cur_bg = COL_DEFAULT;
static int scroll_top = -1, scroll_bot = -1; /* DECSTBM，-1 = 整屏 */
static int saved_cx, saved_cy;
static int dirty_all = 1;
static int dirty_min = INT_MAX, dirty_max = -1;
static int win_w, win_h;

static x11_t c;
static uint32_t win, gc, font;
static int fdm = -1; /* pty 主端 */
static pid_t child;

/* ---------- 网格与滚动 ---------- */

static void cell_clear(cell_t* p) {
  p->ch = ' ';
  p->fg = COL_DEFAULT;
  p->bg = COL_DEFAULT;
}

static void mark_dirty(int y) {
  if (y < dirty_min) {
    dirty_min = y;
  }
  if (y > dirty_max) {
    dirty_max = y;
  }
}

static int scroll_top_edge(void) { return scroll_top < 0 ? 0 : scroll_top; }
static int scroll_bot_edge(void) {
  return scroll_bot < 0 ? rows - 1 : scroll_bot;
}

/* 滚动区内上滚一行（LF 越过底边时） */
static void scroll_up(void) {
  int top = scroll_top_edge();
  int bot = scroll_bot_edge();
  int y;
  if (bot > top) {
    memmove(&grid[top * cols], &grid[(top + 1) * cols],
            (size_t)(bot - top) * cols * sizeof(cell_t));
  }
  for (y = 0; y < cols; y++) {
    cell_clear(&grid[bot * cols + y]);
  }
  for (y = top; y <= bot; y++) {
    mark_dirty(y);
  }
}

/* 滚动区内下滚一行（ESC M 顶行反转） */
static void scroll_down(void) {
  int top = scroll_top_edge();
  int bot = scroll_bot_edge();
  int y;
  if (bot > top) {
    memmove(&grid[(top + 1) * cols], &grid[top * cols],
            (size_t)(bot - top) * cols * sizeof(cell_t));
  }
  for (y = 0; y < cols; y++) {
    cell_clear(&grid[top * cols + y]);
  }
  for (y = top; y <= bot; y++) {
    mark_dirty(y);
  }
}

static void term_nl(void) {
  if (cy == scroll_bot_edge()) {
    scroll_up();
  } else if (cy < rows - 1) {
    cy++;
  }
  mark_dirty(cy);
}

/* 窗口尺寸变化：重开网格，旧内容按左上角对齐保留 */
static void term_resize(int nw, int nh) {
  int nc = (nw - 2 * PAD) / CH_W;
  int nr = (nh - 2 * PAD) / CH_H;
  cell_t* ng;
  int y, keep_r, keep_c;
  if (nc < MIN_COLS) {
    nc = MIN_COLS;
  }
  if (nr < MIN_ROWS) {
    nr = MIN_ROWS;
  }
  if (nc == cols && nr == rows) {
    return;
  }
  ng = (cell_t*)malloc((size_t)nc * nr * sizeof(cell_t));
  if (ng == NULL) {
    return;
  }
  keep_r = nr < rows ? nr : rows;
  keep_c = nc < cols ? nc : cols;
  for (y = 0; y < nr; y++) {
    int x;
    for (x = 0; x < nc; x++) {
      cell_clear(&ng[y * nc + x]);
    }
  }
  for (y = 0; y < keep_r; y++) {
    memcpy(&ng[y * nc], &grid[y * cols], (size_t)keep_c * sizeof(cell_t));
  }
  free(grid);
  grid = ng;
  cols = nc;
  rows = nr;
  if (cx >= cols) {
    cx = cols - 1;
  }
  if (cy >= rows) {
    cy = rows - 1;
  }
  scroll_top = -1;
  scroll_bot = -1;
  dirty_all = 1;
}

/* ---------- 转义序列解析 ---------- */

enum { ST_GROUND = 0, ST_ESC, ST_CSI, ST_OSC, ST_OSC_ESC, ST_CHARSET };

static int st;
static int csi_priv;     /* 收到 '?'（私有序列） */
static int csi_p[16];    /* 参数，-1 = 未给 */
static int csi_np;       /* 当前参数下标，-1 = 还没有任何参数 */

static void csi_dispatch(int fin);

/* 参数取值：未给（或给 0）用默认值。ED/EL 的 0 有意义，由调用方直接读 csi_p */
static int csi_get(int i, int def) {
  if (i > 15) {
    return def;
  }
  if (i > csi_np || csi_p[i] < 0) {
    return def;
  }
  return csi_p[i] == 0 ? def : csi_p[i];
}

static void term_reset(void) {
  int y;
  for (y = 0; y < rows; y++) {
    int x;
    for (x = 0; x < cols; x++) {
      cell_clear(&grid[y * cols + x]);
    }
    mark_dirty(y);
  }
  cx = cy = 0;
  cur_fg = COL_DEFAULT;
  cur_bg = COL_DEFAULT;
  scroll_top = -1;
  scroll_bot = -1;
  cur_visible = 1;
}

static void put_char(uint8_t ch) {
  cell_t* p;
  if (cx >= cols) { /* 自动换行 */
    cx = 0;
    term_nl();
  }
  p = &grid[cy * cols + cx];
  p->ch = ch;
  p->fg = cur_fg;
  p->bg = cur_bg;
  cx++;
  mark_dirty(cy);
}

static void esc_dispatch(int b) {
  switch (b) {
    case '7': /* DECSC 保存光标 */
      saved_cx = cx;
      saved_cy = cy;
      break;
    case '8': /* DECRC 恢复光标 */
      cx = saved_cx < cols ? saved_cx : cols - 1;
      cy = saved_cy < rows ? saved_cy : rows - 1;
      mark_dirty(cy);
      break;
    case 'D': /* IND 下移一行（越底边滚动） */
      term_nl();
      break;
    case 'E': /* NEL 回车 + 下移 */
      cx = 0;
      term_nl();
      break;
    case 'M': /* RI 上移一行（越顶边反转滚动） */
      if (cy == scroll_top_edge()) {
        scroll_down();
      } else if (cy > 0) {
        cy--;
      }
      mark_dirty(cy);
      break;
    case 'c': /* RIS 全复位 */
      term_reset();
      break;
    default: /* '=' '>' 字符集等：忽略 */
      break;
  }
}

/* 256 色索引 → 最近似的 16 调色板下标（不维护独立 256 色表） */
static int nearest16(int r, int g, int b) {
  int best = 0;
  long bestd = -1;
  int i;
  for (i = 0; i < 16; i++) {
    long dr = (long)((palette[i] >> 16) & 0xff) - r;
    long dg = (long)((palette[i] >> 8) & 0xff) - g;
    long db = (long)(palette[i] & 0xff) - b;
    long d = dr * dr + dg * dg + db * db;
    if (bestd < 0 || d < bestd) {
      bestd = d;
      best = i;
    }
  }
  return best;
}

static int map256(int n) {
  int r, g, b;
  if (n < 16) {
    return n;
  }
  if (n >= 232) { /* 24 级灰阶 */
    int v = (n - 232) * 10 + 8;
    return nearest16(v, v, v);
  }
  n -= 16;
  r = (n / 36) * 51;
  g = ((n / 6) % 6) * 51;
  b = (n % 6) * 51;
  return nearest16(r, g, b);
}

static void sgr(void) {
  int i;
  int last = csi_np < 0 ? 0 : csi_np;
  if (csi_np < 0) { /* SGR 无参数 = 复位 */
    cur_fg = COL_DEFAULT;
    cur_bg = COL_DEFAULT;
    return;
  }
  for (i = 0; i <= last; i++) {
    int v = csi_p[i] < 0 ? 0 : csi_p[i];
    if (v == 0) {
      cur_fg = COL_DEFAULT;
      cur_bg = COL_DEFAULT;
    } else if (v == 39) {
      cur_fg = COL_DEFAULT;
    } else if (v == 49) {
      cur_bg = COL_DEFAULT;
    } else if (v >= 30 && v <= 37) {
      cur_fg = (uint8_t)(v - 30);
    } else if (v >= 40 && v <= 47) {
      cur_bg = (uint8_t)(v - 40);
    } else if (v >= 90 && v <= 97) {
      cur_fg = (uint8_t)(v - 90 + 8);
    } else if (v >= 100 && v <= 107) {
      cur_bg = (uint8_t)(v - 100 + 8);
    } else if ((v == 38 || v == 48) && i + 2 <= last && csi_p[i + 1] == 5) {
      int idx = map256(csi_p[i + 2]);
      i += 2;
      if (v == 38) {
        cur_fg = (uint8_t)idx;
      } else {
        cur_bg = (uint8_t)idx;
      }
    }
    /* 1/2/4/7 等属性（粗体/下划线/反显）忽略：fixed 只有一套字形 */
  }
}

static void csi_dispatch(int fin) {
  int n1 = csi_get(0, 1);
  int y, x;
  switch (fin) {
    case 'A': /* CUU 上移 */
      y = cy - n1;
      if (y < scroll_top_edge()) {
        y = scroll_top_edge();
      }
      if (y < 0) {
        y = 0;
      }
      cy = y;
      mark_dirty(cy);
      break;
    case 'B': /* CUD 下移 */
      y = cy + n1;
      if (y > scroll_bot_edge()) {
        y = scroll_bot_edge();
      }
      if (y > rows - 1) {
        y = rows - 1;
      }
      cy = y;
      mark_dirty(cy);
      break;
    case 'C': /* CUF 右移 */
      x = cx + n1;
      cx = x < cols - 1 ? x : cols - 1;
      mark_dirty(cy);
      break;
    case 'D': /* CUB 左移 */
      x = cx - n1;
      cx = x > 0 ? x : 0;
      mark_dirty(cy);
      break;
    case 'G': /* CHA 列绝对 */
    case '`':
      x = csi_get(0, 1) - 1;
      cx = x < 0 ? 0 : (x < cols - 1 ? x : cols - 1);
      mark_dirty(cy);
      break;
    case 'd': /* VPA 行绝对 */
      y = csi_get(0, 1) - 1;
      cy = y < 0 ? 0 : (y < rows - 1 ? y : rows - 1);
      mark_dirty(cy);
      break;
    case 'H': /* CUP 行列绝对（1 基） */
    case 'f':
      y = csi_get(0, 1) - 1;
      x = csi_get(1, 1) - 1;
      cy = y < 0 ? 0 : (y < rows - 1 ? y : rows - 1);
      cx = x < 0 ? 0 : (x < cols - 1 ? x : cols - 1);
      mark_dirty(cy);
      break;
    case 'J': /* ED 清屏 */
      if (csi_np < 0 || csi_p[0] <= 0) { /* 光标到屏尾 */
        for (x = cx; x < cols; x++) {
          cell_clear(&grid[cy * cols + x]);
        }
        for (y = cy + 1; y < rows; y++) {
          int xx;
          for (xx = 0; xx < cols; xx++) {
            cell_clear(&grid[y * cols + xx]);
          }
          mark_dirty(y);
        }
        mark_dirty(cy);
      } else if (csi_p[0] == 1) { /* 屏头到光标 */
        for (y = 0; y < cy; y++) {
          int xx;
          for (xx = 0; xx < cols; xx++) {
            cell_clear(&grid[y * cols + xx]);
          }
          mark_dirty(y);
        }
        for (x = 0; x <= cx && x < cols; x++) {
          cell_clear(&grid[cy * cols + x]);
        }
        mark_dirty(cy);
      } else { /* 2/3 = 全清 */
        term_reset();
      }
      break;
    case 'K': /* EL 清行 */
      if (csi_np < 0 || csi_p[0] <= 0) {
        for (x = cx; x < cols; x++) {
          cell_clear(&grid[cy * cols + x]);
        }
      } else if (csi_p[0] == 1) {
        for (x = 0; x <= cx && x < cols; x++) {
          cell_clear(&grid[cy * cols + x]);
        }
      } else {
        for (x = 0; x < cols; x++) {
          cell_clear(&grid[cy * cols + x]);
        }
      }
      mark_dirty(cy);
      break;
    case 'X': /* ECH 光标处擦 n 格 */
      for (x = cx; x < cols && x < cx + n1; x++) {
        cell_clear(&grid[cy * cols + x]);
      }
      mark_dirty(cy);
      break;
    case 'm': /* SGR 颜色 */
      sgr();
      break;
    case 'r': { /* DECSTBM 滚动区 */
      int top = csi_get(0, 1) - 1;
      int bot = csi_get(1, rows) - 1;
      if (top < 0) {
        top = 0;
      }
      if (bot > rows - 1) {
        bot = rows - 1;
      }
      if (top < bot) {
        scroll_top = top;
        scroll_bot = bot;
      } else {
        scroll_top = -1;
        scroll_bot = -1;
      }
      cx = cy = 0;
      mark_dirty(cy);
      break;
    }
    case 's': /* 保存光标（ANSI.SYS 用法） */
      saved_cx = cx;
      saved_cy = cy;
      break;
    case 'u': /* 恢复光标 */
      cx = saved_cx < cols ? saved_cx : cols - 1;
      cy = saved_cy < rows ? saved_cy : rows - 1;
      mark_dirty(cy);
      break;
    case 'h':
    case 'l':
      if (csi_priv && csi_get(0, 0) == 25) { /* 光标显隐 */
        cur_visible = (fin == 'h');
        mark_dirty(cy);
      }
      break;
    default: /* L/M/P/T 等行编辑、模式切换：忽略 */
      break;
  }
}

static void feed(uint8_t b) {
  switch (st) {
    case ST_GROUND:
      if (b == 0x1b) {
        st = ST_ESC;
      } else if (b == '\r') {
        cx = 0;
        mark_dirty(cy);
      } else if (b == '\n' || b == 0x0b || b == 0x0c) {
        term_nl();
      } else if (b == '\b') {
        if (cx > 0) {
          cx--;
        }
        mark_dirty(cy);
      } else if (b == '\t') {
        int nx = ((cx / 8) + 1) * 8;
        cx = nx < cols - 1 ? nx : cols - 1;
        mark_dirty(cy);
      } else if (b >= 0x20) { /* BEL/NUL/charset 切换等控制码忽略 */
        put_char(b);
      }
      break;
    case ST_ESC:
      if (b == '[') {
        int i;
        st = ST_CSI;
        csi_priv = 0;
        csi_np = -1;
        for (i = 0; i < 16; i++) {
          csi_p[i] = -1;
        }
      } else if (b == ']') {
        st = ST_OSC; /* 标题等 OSC 串：吞到 BEL / ESC\ */
      } else if (b == '(' || b == ')') {
        st = ST_CHARSET; /* 字符集指定：下一个字节忽略 */
      } else {
        esc_dispatch(b);
        st = ST_GROUND;
      }
      break;
    case ST_CSI:
      if (b >= '0' && b <= '9') {
        if (csi_np < 0) {
          csi_np = 0;
        }
        if (csi_np < 16) {
          csi_p[csi_np] =
              (csi_p[csi_np] < 0 ? 0 : csi_p[csi_np]) * 10 + (b - '0');
        }
      } else if (b == ';') {
        if (csi_np < 0) {
          csi_np = 0;
        }
        if (csi_np < 15) {
          csi_np++;
        }
      } else if (b == '?') {
        csi_priv = 1;
      } else if (b >= 0x40 && b <= 0x7e) {
        csi_dispatch(b);
        st = ST_GROUND;
      }
      /* 0x20-0x2f 中间字节：忽略 */
      break;
    case ST_OSC:
      if (b == 0x07) {
        st = ST_GROUND;
      } else if (b == 0x1b) {
        st = ST_OSC_ESC; /* 等 '\' */
      }
      break;
    case ST_OSC_ESC:
      st = ST_GROUND; /* '\'（ST）已到，序列结束 */
      break;
    case ST_CHARSET:
      st = ST_GROUND;
      break;
    default:
      st = ST_GROUND;
      break;
  }
}

/* ---------- 渲染 ---------- */

static void draw_row(int y) {
  cell_t* rowp = &grid[y * cols];
  int py = PAD + y * CH_H;
  int x = 0;
  static uint8_t run[240];
  x11_set_gc(&c, gc, 0x0C /* FG|BG */, BG_DEFAULT, BG_DEFAULT);
  x11_fill_rect(&c, win, gc, 0, py, win_w, CH_H);
  while (x < cols) {
    uint8_t fg = rowp[x].fg;
    uint8_t bg = rowp[x].bg;
    uint32_t fgc, bgc;
    int x1 = x;
    int n;
    while (x1 < cols && rowp[x1].fg == fg && rowp[x1].bg == bg &&
           x1 - x < (int)sizeof(run)) {
      x1++;
    }
    n = x1 - x;
    for (x1 = 0; x1 < n; x1++) {
      run[x1] = rowp[x + x1].ch ? rowp[x + x1].ch : ' ';
    }
    fgc = (fg == COL_DEFAULT) ? FG_DEFAULT : palette[fg];
    bgc = (bg == COL_DEFAULT) ? BG_DEFAULT : palette[bg];
    /* ImageText8 按 GC 背景色填充字形底 ⇒ 非默认背景色也在这条请求里完成 */
    x11_set_gc(&c, gc, 0x0C, fgc, bgc);
    x11_image_text8(&c, win, gc, PAD + x * CH_W, py + BASELINE, (char*)run, n);
    x += n;
  }
  if (cur_visible && y == cy) { /* 光标：实心块 + 原前景色的字符 */
    int px = PAD + cx * CH_W;
    uint8_t fg = rowp[cx].fg;
    char ch = rowp[cx].ch ? (char)rowp[cx].ch : ' ';
    x11_set_gc(&c, gc, 0x0C, CURSOR_COLOR, CURSOR_COLOR);
    x11_fill_rect(&c, win, gc, px, py, CH_W, CH_H);
    x11_set_gc(&c, gc, 0x0C, fg == COL_DEFAULT ? FG_DEFAULT : palette[fg],
               CURSOR_COLOR);
    x11_image_text8(&c, win, gc, px, py + BASELINE, &ch, 1);
  }
}

static void redraw(void) {
  int y;
  if (dirty_all) {
    x11_set_gc(&c, gc, 0x0C, BG_DEFAULT, BG_DEFAULT);
    x11_fill_rect(&c, win, gc, 0, 0, win_w, win_h);
    dirty_all = 0;
    dirty_min = 0;
    dirty_max = rows - 1;
  }
  if (dirty_min > dirty_max) {
    return;
  }
  if (cy < dirty_min) {
    dirty_min = cy; /* 光标行永远跟着重画，移动光标零额外状态 */
  }
  if (cy > dirty_max) {
    dirty_max = cy;
  }
  for (y = dirty_min; y <= dirty_max && y < rows; y++) {
    if (y >= 0) {
      draw_row(y);
    }
  }
  dirty_min = INT_MAX;
  dirty_max = -1;
}

/* ---------- 键盘 ---------- */

#define KC_MIN 8
#define KC_COUNT 248
#define KS_MAX_PER 8

static uint32_t ksmap[KC_COUNT * KS_MAX_PER];
static int ks_per; /* 0 = 未取到（键盘不可用） */

static int load_keymap(void) {
  int per = x11_get_keyboard_mapping(&c, KC_MIN, KC_COUNT, ksmap,
                                     KC_COUNT * KS_MAX_PER);
  if (per > 0 && per <= KS_MAX_PER) {
    ks_per = per;
    return 0;
  }
  return -1;
}

static int put_seq(uint8_t* out, int n, const char* s) {
  while (*s != '\0') {
    out[n++] = (uint8_t)*s++;
  }
  return n;
}

/* keysym → 字节串（写进 pty 主端）。返回字节数，0 = 不认识的键 */
static int key_to_bytes(uint32_t ks, int ctrl, int alt, uint8_t* out) {
  int n = 0;
  if (alt) {
    out[n++] = 0x1b; /* Meta 前缀 */
  }
  switch (ks) {
    case 0xff08: /* BackSpace：按终端惯例送 DEL */
      out[n++] = 0x7f;
      return n;
    case 0xff09: /* Tab */
    case 0xff89:
      out[n++] = '\t';
      return n;
    case 0xff0d: /* Return / KP Enter */
    case 0xff8d:
      out[n++] = '\r';
      return n;
    case 0xff1b: /* Escape */
      out[n++] = 0x1b;
      return n;
    case 0xff50:
      return put_seq(out, n, "\x1b[H"); /* Home */
    case 0xff51:
      return put_seq(out, n, "\x1b[D"); /* Left */
    case 0xff52:
      return put_seq(out, n, "\x1b[A"); /* Up */
    case 0xff53:
      return put_seq(out, n, "\x1b[C"); /* Right */
    case 0xff54:
      return put_seq(out, n, "\x1b[B"); /* Down */
    case 0xff55:
      return put_seq(out, n, "\x1b[5~"); /* Prior */
    case 0xff56:
      return put_seq(out, n, "\x1b[6~"); /* Next */
    case 0xff57:
      return put_seq(out, n, "\x1b[F"); /* End */
    case 0xff63:
      return put_seq(out, n, "\x1b[2~"); /* Insert */
    case 0xffff:
      return put_seq(out, n, "\x1b[3~"); /* Delete */
    default:
      break;
  }
  if (ks >= 0xffbe && ks <= 0xffc7) { /* F1..F10 */
    static const char* fseq[10] = {"\x1b[11~", "\x1b[12~", "\x1b[13~",
                                   "\x1b[14~", "\x1b[15~", "\x1b[17~",
                                   "\x1b[18~", "\x1b[19~", "\x1b[20~",
                                   "\x1b[21~"};
    return put_seq(out, n, fseq[ks - 0xffbe]);
  }
  if (ks >= 0xffb0 && ks <= 0xffb9) { /* KP 0-9 */
    out[n++] = (uint8_t)('0' + (ks - 0xffb0));
    return n;
  }
  if (ks == 0xffae) { /* KP . */
    out[n++] = '.';
    return n;
  }
  if (ks == 0xffaa) {
    out[n++] = '*';
    return n;
  }
  if (ks == 0xffab) {
    out[n++] = '+';
    return n;
  }
  if (ks == 0xffad) {
    out[n++] = '-';
    return n;
  }
  if (ks == 0xffaf) {
    out[n++] = '/';
    return n;
  }
  if (ctrl && ks >= 0x40 && ks <= 0x7f) { /* Ctrl+键 → 控制码（? → DEL） */
    out[n++] = (ks == '?') ? 0x7f : (uint8_t)(ks & 0x1f);
    return n;
  }
  if (ks >= 0x20 && ks < 0x7f) { /* 可打印 ASCII（shift 由 keysym 列区分） */
    out[n++] = (uint8_t)ks;
    return n;
  }
  if (ks >= 0xa0 && ks <= 0xff) { /* Latin-1 → UTF-8 两字节 */
    out[n++] = (uint8_t)(0xc0 | (ks >> 6));
    out[n++] = (uint8_t)(0x80 | (ks & 0x3f));
    return n;
  }
  return 0;
}

static void on_key(const uint8_t* ev) {
  int kc = ev[1]; /* detail = keycode */
  unsigned state;
  int shift, ctrl, alt;
  uint32_t ks;
  uint8_t out[16];
  int n;
  if (kc < KC_MIN || kc >= KC_MIN + KC_COUNT || ks_per <= 0) {
    return;
  }
  state = (unsigned)ev[28] | ((unsigned)ev[29] << 8);
  shift = (int)(state & 1);
  ctrl = (int)(state & 4);
  alt = (state & 8) ? 1 : 0;
  ks = ksmap[(kc - KC_MIN) * ks_per];
  if (shift && ks_per > 1) { /* 第二列 = shift 值；空则退回第一列 */
    uint32_t s = ksmap[(kc - KC_MIN) * ks_per + 1];
    if (s != 0) {
      ks = s;
    }
  }
  n = key_to_bytes(ks, ctrl, alt, out);
  if (n > 0 && fdm >= 0) {
    write(fdm, out, (size_t)n);
  }
}

/* ---------- pty 会话 ---------- */

/* 打开 pty 主端并 fork 出跑 /bin/sh 的子进程；成功返回 0 */
static int spawn_shell(void) {
  char spath[64];
  int idx;
  fdm = open("/dev/ptm", O_RDWR);
  if (fdm < 0) {
    fprintf(stderr, "xterm: open /dev/ptm 失败（pty 模块没起来？）\n");
    return -1;
  }
  ioctl(fdm, IOC_MASTER_READ_NOBLOCK); /* 主端恒非阻塞（兼容老语义） */
  idx = ioctl(fdm, IOC_SLAVE); /* 从端号 = /dev/pts/<idx>（开主端时已创建） */
  snprintf(spath, sizeof(spath), "/dev/pts/%d", idx);
  child = fork();
  if (child < 0) {
    fprintf(stderr, "xterm: fork 失败\n");
    close(fdm);
    fdm = -1;
    return -1;
  }
  if (child == 0) {
    int sfd = open(spath, O_RDWR);
    if (sfd < 0) {
      fprintf(stderr, "xterm: 打不开 %s\n", spath);
      _exit(127);
    }
    dup2(sfd, 0);
    dup2(sfd, 1);
    dup2(sfd, 2);
    if (sfd > 2) {
      close(sfd);
    }
    close(fdm);
    /* 【必须判 >2】-t 文本模式没连 X，c.fd 是 0（静态结构清零）⇒ 无条件
     * close 会把子进程的 stdin 关掉；exec 看到 fds[0]==NULL 又会把 0/1/2
     * 整组换回全局 /dev/std* ⇒ shell 去读串口、EOF 自旋，pty 永远没输入。 */
    if (c.fd > 2) {
      close(c.fd); /* X 连接不能漏给 shell，否则关窗后 X 侧连接悬着 */
    }
    execl("/bin/sh", "sh", (char*)NULL);
    _exit(127);
  }
  return 0;
}

static void drain_pty(void) {
  uint8_t buf[512];
  for (;;) {
    ssize_t n = read(fdm, buf, sizeof(buf));
    ssize_t i;
    if (n <= 0) {
      break; /* -EAGAIN（没数据）/ 0：本轮结束 */
    }
    for (i = 0; i < n; i++) {
      feed(buf[i]);
    }
  }
}

/* 文本模式（xterm -t cmd...）：不连 X，把命令写进 pty 主端、子 shell 的
 * 输出原样倒到 stdout（串口控制台），用来在没有图形环境时验证 pty 链路
 * （主端写 → 从端阻塞读 / 从端写 → 主端非阻塞读）。跑 X11_TTY_MS 毫秒后
 * 收尾退出（交互 shell 不会自己退出）。
 * 【计时必须用 clock_gettime】本内核的 usleep/nanosleep 基本不睡，按
 * "睡了多少次 × 20ms" 累计会把 30 秒窗口瞬间走完（实测 xterm 秒退）。 */
#define X11_TTY_MS 30000

static long long now_ms(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    return 0;
  }
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
static int text_mode(int argc, char* argv[]) {
  uint8_t buf[512];
  char cmd[512];
  int i, n = 0;
  if (spawn_shell() < 0) {
    return 1;
  }
  for (i = 2; i < argc; i++) {
    int len = (int)strlen(argv[i]);
    if (n + len + 2 >= (int)sizeof(cmd)) {
      break;
    }
    if (n > 0) {
      cmd[n++] = ' ';
    }
    memcpy(cmd + n, argv[i], (size_t)len);
    n += len;
  }
  if (n > 0) {
    cmd[n++] = '\r';
    fprintf(stderr, "xterm: ptm=%d child=%d 写入 %d 字节\n", fdm, (int)child, n);
    write(fdm, cmd, (size_t)n);
  }
  {
    long long t0 = now_ms();
    for (;;) {
      ssize_t k = read(fdm, buf, sizeof(buf));
      int status = 0;
      if (k > 0) {
        write(1, buf, (size_t)k); /* 子 shell 的输出原样倒到 stdout */
      } else if (k < 0 && errno != EAGAIN) {
        fprintf(stderr, "xterm: master read err=%d errno=%d\n", (int)k, errno);
        break;
      }
      if (waitpid(child, &status, WNOHANG) == child) {
        fprintf(stderr, "xterm: child exited status=%x\n", status);
        break; /* shell 退出（非交互用法） */
      }
      if (now_ms() - t0 >= X11_TTY_MS) {
        break;
      }
      usleep(20000);
    }
  }
  kill(child, 9);
  for (;;) { /* 倒净残留输出 */
    ssize_t k = read(fdm, buf, sizeof(buf));
    if (k <= 0) {
      break;
    }
    write(1, buf, (size_t)k);
  }
  return 0;
}

int main(int argc, char* argv[]) {
  uint8_t ev[32];
  int gx = -1, gy = -1, x, y;
  uint32_t del_type = 0, del_data = 0;
  int gw = 0, gh = 0;
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  if (argc > 1 && strcmp(argv[1], "-t") == 0) {
    return text_mode(argc, argv); /* 文本模式：不连 X，验证 pty 链路 */
  }

  if (argc > 1 && x11_parse_geom(argv[1], &gw, &gh, &gx, &gy) == 0) {
    int nc = (gw - 2 * PAD) / CH_W;
    int nr = (gh - 2 * PAD) / CH_H;
    if (nc >= MIN_COLS && nc <= 200) {
      cols = nc;
    }
    if (nr >= MIN_ROWS && nr <= 100) {
      rows = nr;
    }
  }
  grid = (cell_t*)malloc((size_t)cols * rows * sizeof(cell_t));
  if (grid == NULL) {
    return 1;
  }
  term_reset();

  if (x11_connect(&c, NULL) < 0) {
    return 1;
  }
  x11_install_arrow_cursor(&c);
  gc = x11_id(&c);
  font = x11_id(&c);
  if (x11_open_font(&c, font, "fixed") < 0 ||
      x11_create_gc(&c, gc, c.root) < 0) {
    fprintf(stderr, "xterm: GC/字体初始化失败\n");
    return 1;
  }
  x11_set_gc_font(&c, gc, font);

  win_w = cols * CH_W + 2 * PAD;
  win_h = rows * CH_H + 2 * PAD;
  x = (gx >= 0) ? gx : (c.w - win_w) / 2;
  y = (gy >= 0) ? gy : (c.h / 8);
  if (x < 0) {
    x = 0;
  }
  if (y < 0) {
    y = 0;
  }
  win = x11_create_window(&c, x, y, win_w, win_h, BG_DEFAULT, 0,
                          X11_EV_EXPOSURE | X11_EV_KEY_PRESS |
                              X11_EV_STRUCTURE);
  if (win == 0) {
    fprintf(stderr, "xterm: 建窗口失败\n");
    return 1;
  }
  x11_set_title(&c, win, "xterm");
  (void)x11_enable_delete_window(&c, win, &del_type, &del_data);
  x11_map_window(&c, win);

  if (spawn_shell() < 0) {
    return 1;
  }
  if (load_keymap() < 0) {
    fprintf(stderr, "xterm: 取键盘映射失败，键盘不可用\n");
  }

  for (;;) {
    struct pollfd pfd;
    int pr;
    int got = 0;
    int quit = 0;
    {
      /* shell 死了就收尸关窗（WNOHANG 不挡输入） */
      int status = 0;
      pid_t r = waitpid(child, &status, WNOHANG);
      if (r == child) {
        static const char msg[] = "\r\n[shell 已退出]\r\n";
        const char* p;
        for (p = msg; *p != '\0'; p++) {
          feed((uint8_t)*p);
        }
        redraw();
        return 0;
      }
    }
    drain_pty();
    pfd.fd = c.fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    pr = poll(&pfd, 1, 25);
    if (pr > 0) {
      int r;
      while ((r = x11_next_event(&c, ev)) == 1) {
        got++;
        /* type 最高位 = send_event 标志（wm 的关闭请求带这一位） */
        if ((ev[0] & 0x7f) == X11_CLIENT_MESSAGE && del_type != 0 &&
            x11_ev_u32(ev, 8) == del_type && x11_ev_u32(ev, 12) == del_data) {
          quit = 1;
          break;
        }
        if (ev[0] == X11_EXPOSE || ev[0] == X11_MAP_NOTIFY) {
          if (x11_ev_u32(ev, 4) == win) {
            dirty_all = 1;
          }
        } else if (ev[0] == X11_CONFIGURE_NOTIFY) {
          int nw = (int)ev[20] | ((int)ev[21] << 8);
          int nh = (int)ev[22] | ((int)ev[23] << 8);
          if (nw > 0 && nh > 0) {
            win_w = nw;
            win_h = nh;
            term_resize(nw, nh);
          }
        } else if (ev[0] == 2 /* KeyPress */) {
          on_key(ev);
        } else if (ev[0] == 34 /* MappingNotify：键盘映射变了就重取 */) {
          load_keymap();
        }
      }
      if (quit || r < 0) {
        break; /* 关闭请求 / 连接断开：杀 shell 退出 */
      }
      /* 与 xcalc 同款：内核 poll 偶发假唤醒，让 2ms 压住空转 */
      if (got == 0) {
        usleep(2000);
      }
    }
  }
  kill(child, 9); /* 窗口被关：shell 没了退出点，直接终止 */
  return 0;
}
