/*******************************************************************
 * x11.c — 极简 X11 客户端协议库（实现见 x11.h 顶部说明）。
 ********************************************************************/
#include "x11.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define X11_SOCKET "/tmp/.X11-unix/X0"

/* ChangeWindowAttributes / CreateWindow 的 value-mask 位 */
#define CW_BACK_PIXMAP 0x0001u
#define CW_BACK_PIXEL 0x0002u
#define CW_OVERRIDE_REDIRECT 0x0200u
#define CW_EVENT_MASK 0x0800u

/* GC 的 value-mask 位 */
#define GC_FUNCTION 0x00000010u
#define GC_FOREGROUND 0x00000004u
#define GC_BACKGROUND 0x00000008u
#define GC_GRAPHICS_EXPOSURES 0x00010000u

/* 事件掩码 */
#define EV_EXPOSURE 0x8000u
#define EV_STRUCTURE 0x20000u
#define EV_BUTTON_PRESS 0x0004u

static int eagain(void) { return errno == EAGAIN || errno == EWOULDBLOCK; }

static uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t le32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}
static void put16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}
static void put32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

static int write_all(int fd, const void* buf, size_t n) {
  const uint8_t* p = (const uint8_t*)buf;
  size_t off = 0;
  while (off < n) {
    ssize_t w = write(fd, p + off, n - off);
    if (w > 0) {
      off += (size_t)w;
    } else if (w < 0 && eagain()) {
      continue;
    } else {
      return -1;
    }
  }
  return 0;
}

static int read_exact(int fd, void* buf, size_t n) {
  uint8_t* p = (uint8_t*)buf;
  size_t off = 0;
  while (off < n) {
    ssize_t r = read(fd, p + off, n - off);
    if (r > 0) {
      off += (size_t)r;
    } else if (r == 0) {
      return -1;
    } else if (eagain()) {
      continue;
    } else {
      return -1;
    }
  }
  return 0;
}

uint32_t x11_id(x11_t* c) { return c->id_base | (c->next_id++ & c->id_mask); }

int x11_connect(x11_t* c, const char* path) {
  struct sockaddr_un sa;
  uint8_t setup[12];
  uint8_t hdr[8];
  static uint8_t body[8192];
  unsigned extra, vendor_len, nformats, off;

  memset(c, 0, sizeof(*c));
  if (path == NULL) {
    path = X11_SOCKET;
  }
  c->fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (c->fd < 0) {
    perror("x11: socket");
    return -1;
  }
  memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", path);
  if (connect(c->fd, (struct sockaddr*)&sa,
              sizeof(sa.sun_family) + strlen(sa.sun_path)) < 0) {
    perror("x11: connect");
    close(c->fd);
    c->fd = -1;
    return -1;
  }
  /* 连接建立：'l' + 11.0 + 无鉴权 */
  memset(setup, 0, sizeof(setup));
  setup[0] = 'l';
  put16(setup + 2, 11);
  if (write_all(c->fd, setup, sizeof(setup)) < 0) {
    return -1;
  }
  if (read_exact(c->fd, hdr, sizeof(hdr)) < 0) {
    return -1;
  }
  if (hdr[0] != 1) {
    fprintf(stderr, "x11: setup rejected (status %d)\n", hdr[0]);
    return -1;
  }
  /* 【坑】这个长度字段的单位是 4 字节，不是字节（其余回复的 length 也是，
   * 但那些自己乘了 4，这里最早漏了 ⇒ 只读到 1/4 的回复，屏幕参数全是垃圾：
   * display 255x65280 depth 0）。 */
  extra = le16(hdr + 6) * 4u;
  if (extra > sizeof(body) || read_exact(c->fd, body, extra) < 0) {
    return -1;
  }
  c->id_base = le32(body + 4);
  c->id_mask = le32(body + 8);
  c->next_id = 0;
  vendor_len = le16(body + 16);
  nformats = body[21];
  /* vendor 字符串补到 4 字节边界，然后才是 pixmap formats */
  off = 32 + ((vendor_len + 3u) & ~3u) + nformats * 8;
  if (getenv("X11_DEBUG") != NULL) {
    unsigned k;
    fprintf(stderr, "x11: extra=%u vendor_len=%u nformats=%u off=%u\n", extra,
            vendor_len, nformats, off);
    for (k = 0; k < extra && k < 128; k++) {
      fprintf(stderr, "%02x%s", body[k], (k % 16 == 15) ? "\n" : " ");
    }
    fprintf(stderr, "\n");
  }
  if (off + 40 > sizeof(body)) {
    fprintf(stderr, "x11: bad setup reply\n");
    return -1;
  }
  c->root = le32(body + off);
  c->cmap = le32(body + off + 4);
  c->w = (int)le16(body + off + 20);
  c->h = (int)le16(body + off + 22);
  c->root_visual = le32(body + off + 32);
  c->root_depth = body[off + 38];
  return 0;
}

void x11_close(x11_t* c) {
  if (c != NULL && c->fd >= 0) {
    close(c->fd);
    c->fd = -1;
  }
}

/* 读 32 字节回复，不碰"附加数据"字段。
 * 【坑】定长回复（GetInputFocus/InternAtom 等）里 offset 4 的 length 字段
 * Xorg 会填 1（不是 0，也不是附加字节数），照着它去读 4 字节会永远等不到
 * 数据 ⇒ wm/wall 卡死在 sync 上。定长回复一律用这个。 */
static int read_reply_raw(x11_t* c, uint8_t* out32) {
  uint8_t rep[32];
  if (read_exact(c->fd, rep, sizeof(rep)) < 0) {
    return -1;
  }
  if (out32 != NULL) {
    memcpy(out32, rep, sizeof(rep));
  }
  return 0;
}

/* 读 32 字节回复 + 按 length 字段跳过附加数据（GetProperty/QueryTree 用） */
static int read_reply(x11_t* c, uint8_t* out32) {
  uint8_t rep[32];
  if (read_exact(c->fd, rep, sizeof(rep)) < 0) {
    return -1;
  }
  if (out32 != NULL) {
    memcpy(out32, rep, sizeof(rep));
  }
  {
    unsigned extra = le32(rep + 4) * 4u;
    uint8_t junk[64];
    while (extra > 0) {
      unsigned n = extra > sizeof(junk) ? (unsigned)sizeof(junk) : extra;
      if (read_exact(c->fd, junk, n) < 0) {
        return -1;
      }
      extra -= n;
    }
  }
  return 0;
}

int x11_sync(x11_t* c) {
  uint8_t q[4];
  uint8_t rep[32];
  memset(q, 0, sizeof(q));
  /* GetInputFocus 没有附加数据 ⇒ 整个请求 4 字节，length = 1（X11 请求头
   * 就是 4 字节：opcode/data/length）。写成 8 字节会得到 BadLength。 */
  q[0] = 43;
  put16(q + 2, 1);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return -1;
  }
  if (read_reply_raw(c, rep) < 0) {
    return -1;
  }
  /* 之前排队的请求出错时，读到的不是回复而是 Error(0)：打出错误码，
   * 否则"抢根窗口失败"这类问题会完全静默（比如已有别的 WM 在跑）。 */
  if (rep[0] != 1) {
    /* Error 事件：code(1) seq(2) bad_value(4) minor(2) major(1) */
    fprintf(stderr, "x11: sync got type=%d code=%d major=%d minor=%d bad=%u\n",
            rep[0], rep[1], rep[10], (int)(rep[8] | (rep[9] << 8)),
            le32(rep + 4));
    return -1;
  }
  return 0;
}

int x11_create_pixmap(x11_t* c, uint32_t pid, uint8_t depth, int w, int h) {
  uint8_t q[16];
  memset(q, 0, sizeof(q));
  q[0] = 53;
  q[1] = depth;
  put16(q + 2, 4);
  put32(q + 4, pid);
  put32(q + 8, c->root);
  put16(q + 12, (uint16_t)w);
  put16(q + 14, (uint16_t)h);
  return write_all(c->fd, q, sizeof(q));
}

int x11_free_pixmap(x11_t* c, uint32_t pid) {
  uint8_t q[8];
  memset(q, 0, sizeof(q));
  q[0] = 54;
  put16(q + 2, 2);
  put32(q + 4, pid);
  return write_all(c->fd, q, sizeof(q));
}

int x11_create_gc(x11_t* c, uint32_t gc, uint32_t drawable) {
  uint8_t q[16];
  memset(q, 0, sizeof(q));
  q[0] = 55;
  put16(q + 2, 4);
  put32(q + 4, gc);
  put32(q + 8, drawable);
  put32(q + 12, 0); /* value-mask 空 */
  return write_all(c->fd, q, sizeof(q));
}

int x11_set_gc(x11_t* c, uint32_t gc, uint32_t mask, uint32_t fg, uint32_t bg) {
  uint8_t q[24];
  unsigned n = 0;
  memset(q, 0, sizeof(q));
  q[0] = 56; /* ChangeGC */
  put32(q + 4, gc);
  put32(q + 8, mask);
  if (mask & GC_FOREGROUND) {
    put32(q + 12 + n * 4, fg);
    n++;
  }
  if (mask & GC_BACKGROUND) {
    put32(q + 12 + n * 4, bg);
    n++;
  }
  put16(q + 2, (uint16_t)((12 + n * 4) / 4));
  return write_all(c->fd, q, 12 + n * 4);
}

int x11_set_gc_font(x11_t* c, uint32_t gc, uint32_t font) {
  uint8_t q[16];
  memset(q, 0, sizeof(q));
  q[0] = 56; /* ChangeGC */
  put16(q + 2, 4);
  put32(q + 4, gc);
  put32(q + 8, 0x4000); /* GCFont */
  put32(q + 12, font);
  return write_all(c->fd, q, sizeof(q));
}

int x11_put_image(x11_t* c, uint32_t drawable, uint32_t gc, int dst_x, int dst_y,
                  int w, int rows, const uint8_t* bgrx) {
  uint8_t q[24];
  uint32_t bytes = (uint32_t)w * (uint32_t)rows * 4u;
  memset(q, 0, sizeof(q));
  q[0] = 72;
  q[1] = 2; /* ZPixmap */
  put16(q + 2, (uint16_t)((sizeof(q) + bytes) / 4));
  put32(q + 4, drawable);
  put32(q + 8, gc);
  put16(q + 12, (uint16_t)w);
  put16(q + 14, (uint16_t)rows);
  put16(q + 16, (uint16_t)dst_x);
  put16(q + 18, (uint16_t)dst_y);
  q[20] = 0;
  q[21] = (uint8_t)c->root_depth;
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return -1;
  }
  return write_all(c->fd, bgrx, bytes);
}

int x11_fill_rect(x11_t* c, uint32_t drawable, uint32_t gc, int x, int y,
                  int w, int h) {
  uint8_t q[20];
  memset(q, 0, sizeof(q));
  q[0] = 70; /* PolyFillRectangle */
  put16(q + 2, 5);
  put32(q + 4, drawable);
  put32(q + 8, gc);
  put16(q + 12, (uint16_t)x);
  put16(q + 14, (uint16_t)y);
  put16(q + 16, (uint16_t)w);
  put16(q + 18, (uint16_t)h);
  return write_all(c->fd, q, sizeof(q));
}

int x11_clear(x11_t* c, uint32_t drawable) {
  uint8_t q[16];
  memset(q, 0, sizeof(q));
  q[0] = 61; /* ClearArea：w/h = 0 ⇒ 到边界 */
  put16(q + 2, 4);
  put32(q + 4, drawable);
  return write_all(c->fd, q, sizeof(q));
}

/* 设置窗口背景（pixmap 或像素值，同一个请求字段） */
static int x11_win_bg(x11_t* c, uint32_t win, uint32_t value, uint32_t mask) {
  uint8_t q[12];
  memset(q, 0, sizeof(q));
  q[0] = 2; /* ChangeWindowAttributes：头 12 字节 + 1 个 value = 16 */
  put16(q + 2, 4);
  put32(q + 4, win);
  put32(q + 8, mask);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return -1;
  }
  {
    uint8_t v[4];
    put32(v, value);
    return write_all(c->fd, v, sizeof(v));
  }
}

int x11_set_root_bg(x11_t* c, uint32_t pixmap) {
  return x11_win_bg(c, c->root, pixmap, CW_BACK_PIXMAP);
}

int x11_set_window_bg(x11_t* c, uint32_t win, uint32_t pixel) {
  return x11_win_bg(c, win, pixel, CW_BACK_PIXEL);
}

int x11_select_input(x11_t* c, uint32_t win, uint32_t event_mask) {
  return x11_win_bg(c, win, event_mask, CW_EVENT_MASK);
}

uint32_t x11_create_window(x11_t* c, int x, int y, int w, int h, uint32_t bg,
                           int override_redirect, uint32_t event_mask) {
  uint8_t q[40];
  uint32_t wid = x11_id(c);
  uint32_t mask = CW_BACK_PIXEL | CW_EVENT_MASK;
  unsigned n = 0;
  memset(q, 0, sizeof(q));
  q[0] = 1; /* CreateWindow */
  q[1] = (uint8_t)c->root_depth;
  put32(q + 4, wid);
  put32(q + 8, c->root);
  put16(q + 12, (uint16_t)x);
  put16(q + 14, (uint16_t)y);
  put16(q + 16, (uint16_t)w);
  put16(q + 18, (uint16_t)h);
  put16(q + 20, 0); /* border width */
  put16(q + 22, 1); /* InputOutput */
  put32(q + 24, c->root_visual);
  if (override_redirect) {
    mask |= CW_OVERRIDE_REDIRECT;
  }
  put32(q + 28, mask);
  put32(q + 32 + n * 4, bg); /* 值顺序 = mask 位序（back_pixel 最低） */
  n++;
  if (override_redirect) {
    put32(q + 32 + n * 4, 1);
    n++;
  }
  put32(q + 32 + n * 4, event_mask);
  n++;
  put16(q + 2, (uint16_t)((32 + n * 4) / 4));
  if (write_all(c->fd, q, 32 + n * 4) < 0) {
    return 0;
  }
  return wid;
}

int x11_map_window(x11_t* c, uint32_t win) {
  uint8_t q[8];
  memset(q, 0, sizeof(q));
  q[0] = 8; /* MapWindow */
  put16(q + 2, 2);
  put32(q + 4, win);
  return write_all(c->fd, q, sizeof(q));
}

int x11_unmap_window(x11_t* c, uint32_t win) {
  uint8_t q[8];
  memset(q, 0, sizeof(q));
  q[0] = 10; /* UnmapWindow */
  put16(q + 2, 2);
  put32(q + 4, win);
  return write_all(c->fd, q, sizeof(q));
}

int x11_destroy_window(x11_t* c, uint32_t win) {
  uint8_t q[8];
  memset(q, 0, sizeof(q));
  q[0] = 4; /* DestroyWindow */
  put16(q + 2, 2);
  put32(q + 4, win);
  return write_all(c->fd, q, sizeof(q));
}

int x11_open_font(x11_t* c, uint32_t fid, const char* name) {
  uint8_t q[12];
  unsigned len = (unsigned)strlen(name);
  unsigned pad = (4 - (len & 3)) & 3;
  memset(q, 0, sizeof(q));
  q[0] = 45; /* OpenFont：头 12 字节（fid + name_len）再跟名字 */
  put16(q + 2, (uint16_t)((12 + len + pad) / 4));
  put32(q + 4, fid);
  put16(q + 8, (uint16_t)len);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return -1;
  }
  if (write_all(c->fd, name, len) < 0) {
    return -1;
  }
  if (pad > 0) {
    uint8_t z[4] = {0, 0, 0, 0};
    if (write_all(c->fd, z, pad) < 0) {
      return -1;
    }
  }
  return 0;
}

int x11_close_font(x11_t* c, uint32_t fid) {
  uint8_t q[8];
  memset(q, 0, sizeof(q));
  q[0] = 46; /* CloseFont */
  put16(q + 2, 2);
  put32(q + 4, fid);
  return write_all(c->fd, q, sizeof(q));
}

int x11_image_text8(x11_t* c, uint32_t drawable, uint32_t gc, int x, int y,
                    const char* s, int len) {
  uint8_t q[16];
  unsigned pad = (unsigned)((4 - (len & 3)) & 3);
  memset(q, 0, sizeof(q));
  q[0] = 76;           /* ImageText8 */
  q[1] = (uint8_t)len; /* 第二字节 = 字符串长度，漏了就是 BadLength */
  put16(q + 2, (uint16_t)((16 + (unsigned)len + pad) / 4));
  put32(q + 4, drawable);
  put32(q + 8, gc);
  put16(q + 12, (uint16_t)x);
  put16(q + 14, (uint16_t)y);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return -1;
  }
  if (write_all(c->fd, s, (size_t)len) < 0) {
    return -1;
  }
  if (pad > 0) {
    uint8_t z[4] = {0, 0, 0, 0};
    return write_all(c->fd, z, pad);
  }
  return 0;
}

int x11_grab_pointer(x11_t* c, uint32_t win, uint32_t event_mask) {
  uint8_t q[24];
  memset(q, 0, sizeof(q));
  q[0] = 26; /* GrabPointer */
  q[1] = 0;  /* owner_events = false：事件只发给抓取者 */
  put16(q + 2, 6);
  put32(q + 4, win);
  put16(q + 8, (uint16_t)event_mask);
  q[10] = 1; /* pointer_mode = Async */
  q[11] = 1; /* keyboard_mode = Async */
  put32(q + 12, 0); /* confine_to = None */
  put32(q + 16, 0); /* cursor = None */
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return -1;
  }
  return 0;
}

int x11_ungrab_pointer(x11_t* c) {
  uint8_t q[8];
  memset(q, 0, sizeof(q));
  q[0] = 27; /* UngrabPointer */
  put16(q + 2, 2);
  put32(q + 4, 0); /* time = CurrentTime */
  return write_all(c->fd, q, sizeof(q));
}

uint32_t x11_intern_atom(x11_t* c, const char* name) {
  uint8_t q[8];
  uint8_t rep[32];
  unsigned len = (unsigned)strlen(name);
  unsigned pad = (4 - (len & 3)) & 3;
  memset(q, 0, sizeof(q));
  q[0] = 16; /* InternAtom：头 8 字节再跟名字 */
  q[1] = 0;  /* only_if_exists = false */
  put16(q + 2, (uint16_t)((8 + len + pad) / 4));
  put16(q + 4, (uint16_t)len);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return 0;
  }
  if (write_all(c->fd, name, len) < 0) {
    return 0;
  }
  if (pad > 0) {
    uint8_t z[4] = {0, 0, 0, 0};
    if (write_all(c->fd, z, pad) < 0) {
      return 0;
    }
  }
  if (read_reply_raw(c, rep) < 0) {
    return 0;
  }
  return le32(rep + 8);
}

int x11_set_property32(x11_t* c, uint32_t win, uint32_t prop, uint32_t type,
                       const uint32_t* data, int count) {
  uint8_t q[24];
  int i;
  memset(q, 0, sizeof(q));
  q[0] = 18; /* ChangeProperty */
  q[1] = 0;  /* Replace */
  put16(q + 2, (uint16_t)((24 + count * 4) / 4));
  put32(q + 4, win);
  put32(q + 8, prop);
  put32(q + 12, type);
  q[16] = 32; /* format */
  put32(q + 20, (uint32_t)count);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return -1;
  }
  for (i = 0; i < count; i++) {
    uint8_t v[4];
    put32(v, data[i]);
    if (write_all(c->fd, v, sizeof(v)) < 0) {
      return -1;
    }
  }
  return 0;
}

/* ---------------- 窗口管理器用到的请求 ---------------- */

int x11_change_attrs(x11_t* c, uint32_t win, uint32_t value_mask, uint32_t value) {
  uint8_t q[16];
  memset(q, 0, sizeof(q));
  q[0] = 2;
  put16(q + 2, 4);
  put32(q + 4, win);
  put32(q + 8, value_mask);
  put32(q + 12, value);
  return write_all(c->fd, q, sizeof(q));
}

/* 一次配置 x/y/w/h：mask 按 X/Y/W/H 位序给值，传 -1 表示该项不改 */
int x11_configure(x11_t* c, uint32_t win, int x, int y, int w, int h) {
  uint8_t q[28];
  uint32_t mask = 0;
  unsigned n = 0;
  memset(q, 0, sizeof(q));
  q[0] = 12; /* ConfigureWindow */
  put32(q + 4, win);
  if (x >= 0) {
    mask |= 0x01;
  }
  if (y >= 0) {
    mask |= 0x02;
  }
  if (w > 0) {
    mask |= 0x04;
  }
  if (h > 0) {
    mask |= 0x08;
  }
  put16(q + 8, (uint16_t)mask);
  if (x >= 0) {
    put32(q + 12 + n * 4, (uint32_t)x);
    n++;
  }
  if (y >= 0) {
    put32(q + 12 + n * 4, (uint32_t)y);
    n++;
  }
  if (w > 0) {
    put32(q + 12 + n * 4, (uint32_t)w);
    n++;
  }
  if (h > 0) {
    put32(q + 12 + n * 4, (uint32_t)h);
    n++;
  }
  put16(q + 2, (uint16_t)((12 + n * 4) / 4));
  return write_all(c->fd, q, 12 + n * 4);
}

int x11_raise(x11_t* c, uint32_t win) {
  uint8_t q[16];
  memset(q, 0, sizeof(q));
  q[0] = 12;
  put16(q + 2, 4);
  put32(q + 4, win);
  put16(q + 8, 0x40); /* CWStackMode = Above */
  put32(q + 12, 0);   /* Above */
  return write_all(c->fd, q, sizeof(q));
}

int x11_reparent(x11_t* c, uint32_t win, uint32_t parent, int x, int y) {
  uint8_t q[16];
  memset(q, 0, sizeof(q));
  q[0] = 7;
  put16(q + 2, 4);
  put32(q + 4, win);
  put32(q + 8, parent);
  put16(q + 12, (uint16_t)x);
  put16(q + 14, (uint16_t)y);
  return write_all(c->fd, q, sizeof(q));
}

int x11_send_event(x11_t* c, uint32_t dst, uint32_t event_mask,
                   const uint8_t ev32[32]) {
  uint8_t q[44];
  memset(q, 0, sizeof(q));
  q[0] = 25;
  put16(q + 2, 11);
  put32(q + 4, dst);
  put32(q + 8, event_mask);
  memcpy(q + 12, ev32, 32);
  return write_all(c->fd, q, sizeof(q));
}

int x11_set_focus(x11_t* c, uint32_t win) {
  uint8_t q[12];
  memset(q, 0, sizeof(q));
  q[0] = 42;
  q[1] = 2; /* RevertToParent */
  put16(q + 2, 3);
  put32(q + 4, win);
  put32(q + 8, 0); /* CurrentTime */
  return write_all(c->fd, q, sizeof(q));
}

int x11_add_to_save_set(x11_t* c, uint32_t win) {
  uint8_t q[8];
  memset(q, 0, sizeof(q));
  q[0] = 85;
  put16(q + 2, 2);
  put32(q + 4, win);
  return write_all(c->fd, q, sizeof(q));
}

int x11_kill_client(x11_t* c, uint32_t resource) {
  uint8_t q[8];
  memset(q, 0, sizeof(q));
  q[0] = 113;
  put16(q + 2, 2);
  put32(q + 4, resource);
  return write_all(c->fd, q, sizeof(q));
}

int x11_query_children(x11_t* c, uint32_t win, uint32_t* out, int max) {
  uint8_t q[8];
  uint8_t rep[32];
  int n, i;
  memset(q, 0, sizeof(q));
  q[0] = 15; /* QueryTree */
  put16(q + 2, 2);
  put32(q + 4, win);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return 0;
  }
  if (read_reply(c, rep) < 0) {
    return 0;
  }
  n = (int)le16(rep + 16);
  if (n > max) {
    n = max;
  }
  for (i = 0; i < n; i++) {
    uint8_t v[4];
    if (read_exact(c->fd, v, 4) < 0) {
      return i;
    }
    out[i] = le32(v);
  }
  return n;
}

int x11_get_geometry(x11_t* c, uint32_t win, int* x, int* y, int* w, int* h) {
  uint8_t q[8];
  uint8_t rep[32];
  memset(q, 0, sizeof(q));
  q[0] = 14; /* GetGeometry */
  put16(q + 2, 2);
  put32(q + 4, win);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return -1;
  }
  if (read_reply(c, rep) < 0) {
    return -1;
  }
  if (x) {
    *x = (int16_t)le16(rep + 12);
  }
  if (y) {
    *y = (int16_t)le16(rep + 14);
  }
  if (w) {
    *w = (int)le16(rep + 16);
  }
  if (h) {
    *h = (int)le16(rep + 18);
  }
  return 0;
}

int x11_get_attrs(x11_t* c, uint32_t win, int* map_state, int* override_redirect,
                  uint32_t* all_event_masks) {
  uint8_t q[8];
  uint8_t rep[32];
  memset(q, 0, sizeof(q));
  q[0] = 3; /* GetWindowAttributes */
  put16(q + 2, 2);
  put32(q + 4, win);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return -1;
  }
  if (read_reply(c, rep) < 0) {
    return -1;
  }
  if (map_state) {
    *map_state = rep[26];
  }
  if (override_redirect) {
    *override_redirect = rep[27];
  }
  if (all_event_masks) {
    *all_event_masks = le32(rep + 28);
  }
  return 0;
}

/* 读 32 位格式属性（返回元素个数；0 = 没有/类型不符） */
int x11_get_property32(x11_t* c, uint32_t win, uint32_t prop, uint32_t* out,
                       int max) {
  uint8_t q[24];
  uint8_t rep[32];
  int n32, i;
  memset(q, 0, sizeof(q));
  q[0] = 20; /* GetProperty */
  put16(q + 2, 6);
  put32(q + 4, win);
  put32(q + 8, prop);
  put32(q + 12, 0); /* AnyPropertyType */
  put32(q + 16, 0); /* long offset */
  put32(q + 20, (uint32_t)max);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return 0;
  }
  if (read_reply(c, rep) < 0) {
    return 0;
  }
  if (rep[1] != 32 || le32(rep + 16) == 0) {
    return 0;
  }
  n32 = (int)le32(rep + 16);
  if (n32 > max) {
    n32 = max;
  }
  for (i = 0; i < n32; i++) {
    uint8_t v[4];
    if (read_exact(c->fd, v, 4) < 0) {
      return i;
    }
    out[i] = le32(v);
  }
  return n32;
}

/* 读字符串属性（WM_NAME 之类，format=8），返回字节数 */
int x11_get_property_str(x11_t* c, uint32_t win, uint32_t prop, char* out,
                         int max) {
  uint8_t q[24];
  uint8_t rep[32];
  int nbytes;
  int n;
  int i;
  memset(q, 0, sizeof(q));
  q[0] = 20;
  put16(q + 2, 6);
  put32(q + 4, win);
  put32(q + 8, prop);
  put32(q + 12, 0);
  put32(q + 16, 0);
  put32(q + 20, (uint32_t)max);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return 0;
  }
  if (read_reply(c, rep) < 0) {
    return 0;
  }
  if (rep[1] != 8) {
    return 0;
  }
  nbytes = (int)le32(rep + 16);
  n = nbytes;
  if (n > max - 1) {
    n = max - 1;
  }
  for (i = 0; i < nbytes; i++) {
    uint8_t ch;
    if (read_exact(c->fd, &ch, 1) < 0) {
      return i;
    }
    if (i < n) {
      out[i] = (char)ch;
    }
  }
  out[n] = '\0';
  return n;
}

int x11_delete_property(x11_t* c, uint32_t win, uint32_t prop) {
  uint8_t q[12];
  memset(q, 0, sizeof(q));
  q[0] = 19; /* DeleteProperty */
  put16(q + 2, 3);
  put32(q + 4, win);
  put32(q + 8, prop);
  return write_all(c->fd, q, sizeof(q));
}

int x11_next_event(x11_t* c, uint8_t* ev) {
  ssize_t r = read(c->fd, ev, 32);
  if (r == 32) {
    return 1;
  }
  if (r > 0) { /* 半个事件：补齐 */
    if (read_exact(c->fd, ev + r, (size_t)(32 - r)) < 0) {
      return -1;
    }
    return 1;
  }
  if (r == 0) {
    return -1;
  }
  if (eagain()) {
    return 0;
  }
  return -1;
}
