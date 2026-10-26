/*******************************************************************
 * xcalc — 自研计算器（替代 Xorg 的 xcalc）。
 *
 * 界面就两块：顶部一条显示栏 + 4×5 按键网格。全部用 PolyFillRectangle
 * 和 ImageText8 画，客户端一个像素都不上传 ⇒ 重绘成本只有几十条请求。
 * 鼠标按下的那一格先变亮，40ms 后恢复（给一次视觉反馈）。
 *
 * 【按键字符用 Latin-1 字节写字面量】协议层发的是原始字节，服务器按字体的
 * 编码（misc-fixed 是 ISO-8859-1）解释：在 UTF-8 源码里直接写 "×" 会发出
 * 两个字节（0xC3 0x97），画出来是两个乱码字形。所以 ± × ÷ 一律写 \xb1
 * \xd7 \xf7。
 *
 * 用法：xcalc [WxH[+X+Y]]
 ********************************************************************/
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "x11.h"

#define PAD 8    /* 四周边距 */
#define GAP 6    /* 按键间隙 */
#define DISP_H 44
#define NKEYS 19 /* 16 + 第 5 行的 0(两格) . = */
#define GLYPH_W 6 /* fixed 字体宽 6px（和 panel/dock 同一假设） */
#define PRESS_MS 40

#define WIN_BG X11_RGB(0x22, 0x26, 0x2e)
#define DISP_BG X11_RGB(0x13, 0x16, 0x1b)
#define DISP_FG X11_RGB(0x9c, 0xe8, 0xa8)
#define KEY_BG X11_RGB(0x36, 0x3c, 0x48)
#define KEY_FG X11_RGB(0xec, 0xef, 0xf4)
#define KEY_OP X11_RGB(0x2f, 0x4c, 0x72)
#define KEY_EQ X11_RGB(0xc4, 0x6c, 0x36)
#define KEY_DN X11_RGB(0x6a, 0x7c, 0x9a)

struct kbtn {
  const char* label;
  int x, y, w, h;
};

static x11_t c;
static uint32_t win, gc, font;
static int win_w = 226, win_h = 272; /* 4*48+3*6+2*8 , 8+44+6+5*36+4*6+8 */
static int kw = 48, kh = 36;
static struct kbtn keys[NKEYS];
static int nkeys;

/* 计算状态：acc 是累加器，pend 是还没算的运算符 */
static double acc;
static char pend;
static int have_acc;
static int entering; /* 1 = disp 正在被输入（下一次按数字重新开始） */
static char disp[32] = "0";
static int pressed = -1; /* 正被按下高亮的键下标 */

static void layout(void) {
  static const char* const lbl[5][4] = {
      {"C", "\xb1", "%", "\xf7"},
      {"7", "8", "9", "\xd7"},
      {"4", "5", "6", "-"},
      {"1", "2", "3", "+"},
      {"0", NULL, ".", "="},
  };
  int r, col;
  kw = (win_w - 2 * PAD - 3 * GAP) / 4;
  kh = (win_h - 2 * PAD - DISP_H - GAP - 4 * GAP) / 5;
  if (kw < 20) {
    kw = 20;
  }
  if (kh < 18) {
    kh = 18;
  }
  nkeys = 0;
  for (r = 0; r < 5; r++) {
    for (col = 0; col < 4; col++) {
      int span;
      if (lbl[r][col] == NULL || nkeys >= NKEYS) {
        continue;
      }
      span = (r == 4 && col == 0) ? 2 : 1; /* 0 占两格，像真计算器 */
      keys[nkeys].label = lbl[r][col];
      keys[nkeys].x = PAD + col * (kw + GAP);
      keys[nkeys].y = PAD + DISP_H + GAP + r * (kh + GAP);
      keys[nkeys].w = span * kw + (span - 1) * GAP;
      keys[nkeys].h = kh;
      nkeys++;
    }
  }
}

static uint32_t key_color(const char* s) {
  if (strcmp(s, "=") == 0) {
    return KEY_EQ;
  }
  if (strcmp(s, "+") == 0 || strcmp(s, "-") == 0 || strcmp(s, "\xd7") == 0 ||
      strcmp(s, "\xf7") == 0 || strcmp(s, "%") == 0 ||
      strcmp(s, "\xb1") == 0 || strcmp(s, "C") == 0) {
    return KEY_OP;
  }
  return KEY_BG;
}

static void draw_key(int i, int down) {
  const struct kbtn* k = &keys[i];
  uint32_t bg = down ? KEY_DN : key_color(k->label);
  int len = (int)strlen(k->label);
  x11_set_gc(&c, gc, 0x0C /* FG|BG */, bg, bg);
  x11_fill_rect(&c, win, gc, k->x, k->y, k->w, k->h);
  x11_set_gc(&c, gc, 0x0C, KEY_FG, bg);
  x11_image_text8(&c, win, gc, k->x + (k->w - len * GLYPH_W) / 2,
                  k->y + k->h / 2 + 5, k->label, len);
}

static void draw_display(void) {
  int len = (int)strlen(disp);
  int x = win_w - PAD - 6 - len * GLYPH_W;
  if (x < PAD + 4) {
    x = PAD + 4;
  }
  x11_set_gc(&c, gc, 0x0C, DISP_BG, DISP_BG);
  x11_fill_rect(&c, win, gc, PAD, PAD, win_w - 2 * PAD, DISP_H);
  x11_set_gc(&c, gc, 0x0C, DISP_FG, DISP_BG);
  x11_image_text8(&c, win, gc, x, PAD + DISP_H - 14, disp, len);
}

static void draw_all(void) {
  int i;
  x11_set_gc(&c, gc, 0x0C, WIN_BG, WIN_BG);
  x11_fill_rect(&c, win, gc, 0, 0, win_w, win_h);
  for (i = 0; i < nkeys; i++) {
    draw_key(i, 0);
  }
  draw_display();
}

static void set_err(void) {
  snprintf(disp, sizeof(disp), "Error");
  acc = 0;
  have_acc = 0;
  pend = 0;
  entering = 0;
}

static void set_value(double v) {
  if (v != v || v > 1e300 || v < -1e300) { /* NaN / inf */
    set_err();
    return;
  }
  snprintf(disp, sizeof(disp), "%.10g", v);
  entering = 0;
}

/* 成功返回 0；除零/溢出返回 -1（调用方转成 Error） */
static int calc(char op, double a, double b, double* out) {
  switch (op) {
    case '+':
      *out = a + b;
      break;
    case '-':
      *out = a - b;
      break;
    case '*':
      *out = a * b;
      break;
    case '/':
      if (b == 0.0) {
        return -1;
      }
      *out = a / b;
      break;
    default:
      *out = b;
      break;
  }
  if (*out != *out || *out > 1e300 || *out < -1e300) {
    return -1;
  }
  return 0;
}

static void do_op(char op) {
  double v = atof(disp);
  double r;
  if (pend != 0 && entering) { /* 连算：2 + 3 + ⇒ 先把 2+3 结掉 */
    if (calc(pend, acc, v, &r) < 0) {
      set_err();
      return;
    }
    acc = r;
  } else if (!have_acc) {
    acc = v;
    have_acc = 1;
  } else if (entering) {
    acc = v;
  }
  pend = op;
  set_value(acc);
}

static void do_eq(void) {
  double v = atof(disp);
  double r;
  if (pend != 0) {
    if (calc(pend, acc, v, &r) < 0) {
      set_err();
      return;
    }
    acc = r;
    pend = 0;
    have_acc = 1;
    set_value(acc);
  }
  entering = 0;
}

static void press_key(int i) {
  const char* s = keys[i].label;
  if (s[0] >= '0' && s[0] <= '9' && s[1] == '\0') {
    if (!entering) {
      disp[0] = '\0';
      entering = 1;
    }
    if (strlen(disp) < 14) {
      strcat(disp, s);
    }
  } else if (strcmp(s, ".") == 0) {
    if (!entering) {
      strcpy(disp, "0");
      entering = 1;
    }
    if (strchr(disp, '.') == NULL) {
      strcat(disp, ".");
    }
  } else if (strcmp(s, "C") == 0) {
    strcpy(disp, "0");
    acc = 0;
    have_acc = 0;
    pend = 0;
    entering = 0;
  } else if (strcmp(s, "\xb1") == 0) { /* ± */
    set_value(-atof(disp));
  } else if (strcmp(s, "%") == 0) {
    set_value(atof(disp) / 100.0);
  } else if (strcmp(s, "+") == 0) {
    do_op('+');
  } else if (strcmp(s, "-") == 0) {
    do_op('-');
  } else if (strcmp(s, "\xd7") == 0) { /* × */
    do_op('*');
  } else if (strcmp(s, "\xf7") == 0) { /* ÷ */
    do_op('/');
  } else if (strcmp(s, "=") == 0) {
    do_eq();
  }
  draw_display();
}

static int hit(int x, int y) {
  int i;
  for (i = 0; i < nkeys; i++) {
    if (x >= keys[i].x && x < keys[i].x + keys[i].w && y >= keys[i].y &&
        y < keys[i].y + keys[i].h) {
      return i;
    }
  }
  return -1;
}

int main(int argc, char* argv[]) {
  uint8_t ev[32];
  int gx = -1, gy = -1, x, y;
  uint32_t del_type = 0, del_data = 0;
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  if (argc > 1) {
    int gw = win_w, gh = win_h;
    if (x11_parse_geom(argv[1], &gw, &gh, &gx, &gy) == 0) {
      if (gw >= 140 && gh >= 180) { /* 太小的尺寸按键会挤成一团，忽略 */
        win_w = gw;
        win_h = gh;
      }
    }
  }
  if (x11_connect(&c, NULL) < 0) {
    return 1;
  }
  x11_install_arrow_cursor(&c);
  gc = x11_id(&c);
  font = x11_id(&c);
  if (x11_open_font(&c, font, "fixed") < 0 ||
      x11_create_gc(&c, gc, c.root) < 0) {
    fprintf(stderr, "xcalc: 初始化 GC/字体失败\n");
    return 1;
  }
  x11_set_gc_font(&c, gc, font);
  layout();

  x = (gx >= 0) ? gx : (c.w / 2 - win_w / 2);
  y = (gy >= 0) ? gy : (c.h / 2 - win_h / 2);
  if (x < 0) {
    x = 0;
  }
  if (y < 0) {
    y = 0;
  }
  win = x11_create_window(&c, x, y, win_w, win_h, WIN_BG, 0,
                          X11_EV_EXPOSURE | X11_EV_BUTTON_PRESS |
                              X11_EV_STRUCTURE);
  if (win == 0) {
    fprintf(stderr, "xcalc: 建窗口失败\n");
    return 1;
  }
  x11_set_title(&c, win, "xcalc");
  (void)x11_enable_delete_window(&c, win, &del_type, &del_data);
  x11_map_window(&c, win);
  printf("xcalc: %dx%d at %d,%d，%d 个按键\n", win_w, win_h, x, y, nkeys);
  draw_all();

  for (;;) {
    struct pollfd pfd;
    /* 上一轮按下的高亮在这里恢复：隔一次 poll（40ms）才撤，
     * 否则"按下变色"和"立刻复原"在同一轮里发出去，肉眼看不到。 */
    if (pressed >= 0) {
      draw_key(pressed, 0);
      pressed = -1;
    }
    pfd.fd = c.fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, PRESS_MS) > 0) {
      int r;
      int quit = 0;
      while ((r = x11_next_event(&c, ev)) == 1) {
        /* type 最高位 = send_event 标志（wm 的关闭请求带这一位） */
        if ((ev[0] & 0x7f) == X11_CLIENT_MESSAGE && del_type != 0 &&
            x11_ev_u32(ev, 8) == del_type && x11_ev_u32(ev, 12) == del_data) {
          quit = 1;
          break;
        }
        if (ev[0] == X11_EXPOSE || ev[0] == X11_MAP_NOTIFY) {
          /* MapNotify 也重绘一次：reparent 的 unmap→map 可能把窗口内容丢掉
           * 而不补 Expose（见 xeyes 里同款注释）。 */
          if (x11_ev_u32(ev, 4) == win) {
            draw_all();
          }
        } else if (ev[0] == X11_CONFIGURE_NOTIFY) {
          int nw = (int)ev[20] | ((int)ev[21] << 8);
          int nh = (int)ev[22] | ((int)ev[23] << 8);
          if (nw > 0 && nh > 0 && (nw != win_w || nh != win_h)) {
            win_w = nw;
            win_h = nh;
            layout();
            draw_all();
          }
        } else if (ev[0] == X11_BUTTON_PRESS && ev[1] == 1) {
          if (x11_ev_u32(ev, 12) == win) {
            int i = hit((int16_t)(ev[24] | (ev[25] << 8)),
                        (int16_t)(ev[26] | (ev[27] << 8)));
            if (i >= 0) {
              pressed = i;
              draw_key(i, 1);
              press_key(i);
            }
          }
        }
      }
      if (quit || r < 0) {
        break; /* 关闭请求 / 连接断了：退出进程 */
      }
    }
  }
  return 0;
}
