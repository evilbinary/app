/*******************************************************************
 * x11.c — 极简 X11 客户端协议库（实现见 x11.h 顶部说明）。
 ********************************************************************/
#include "x11.h"

#include <errno.h>
#include <fcntl.h>
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

/* 读 24/32bpp 未压缩 BMP 成 BGRX 像素（与屏帧字节序一致，可直接上传）。
 * 资源都装在镜像的 /res 下，所以先试绝对路径，再试相对当前目录。 */
int x11_load_bmp(const char* name, x11_image_t* img) {
  static uint8_t buf[256 * 1024];
  const char* dirs[2] = {"/res/", "res/"};
  char path[64];
  int fd = -1;
  int i, y;
  long total = 0;
  uint32_t off, stride;
  int w, h, bpp, topdown;
  uint8_t* px;

  if (img == NULL) {
    return -1;
  }
  img->w = 0;
  img->h = 0;
  img->px = NULL;
  for (i = 0; i < 2 && fd < 0; i++) {
    snprintf(path, sizeof(path), "%s%s", dirs[i], name);
    fd = open(path, O_RDONLY);
  }
  if (fd < 0) {
    return -1;
  }
  while (total < (long)sizeof(buf)) {
    ssize_t n = read(fd, buf + total, sizeof(buf) - (size_t)total);
    if (n <= 0) {
      break;
    }
    total += n;
  }
  close(fd);
  if (total < 54 || buf[0] != 'B' || buf[1] != 'M' || le32(buf + 30) != 0) {
    return -1;
  }
  off = le32(buf + 10);
  w = (int)le32(buf + 18);
  h = (int)le32(buf + 22);
  bpp = (int)le16(buf + 28);
  if ((bpp != 24 && bpp != 32) || w <= 0 || w > 512 || h == 0) {
    return -1;
  }
  topdown = (h < 0);
  if (topdown) {
    h = -h;
  }
  if (h > 512) {
    return -1;
  }
  stride = ((uint32_t)w * (uint32_t)(bpp / 8) + 3u) & ~3u;
  if ((long)off + (long)stride * h > total) {
    return -1;
  }
  px = (uint8_t*)malloc((size_t)w * (size_t)h * 4u);
  if (px == NULL) {
    return -1;
  }
  for (y = 0; y < h; y++) {
    int sy = topdown ? y : (h - 1 - y);
    const uint8_t* s = buf + off + (uint32_t)sy * stride;
    uint8_t* d = px + (size_t)y * (size_t)w * 4u;
    int x;
    for (x = 0; x < w; x++, d += 4, s += bpp / 8) {
      d[0] = s[0]; /* BMP 的 BGR(A) 就是屏帧的 BGRX 顺序 */
      d[1] = s[1];
      d[2] = s[2];
      d[3] = 0;
    }
  }
  img->w = w;
  img->h = h;
  img->px = px;
  return 0;
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

int x11_set_window_bg_pixmap(x11_t* c, uint32_t win, uint32_t pixmap) {
  return x11_win_bg(c, win, pixmap, CW_BACK_PIXMAP);
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

/* ---------------- 光标 ----------------
 * 屏幕上的光标位图由谁提供？两条路：
 *   1. server 自己的默认根光标：dix/main.c:230 调 CreateRootCursor()，
 *      打开核心字体 "cursor" 并用它的 0 号字形造光标（dix/cursor.c:491）。
 *      注意取不到字体时 server 是 FatalError 起不来，不是"隐形光标"。
 *   2. 客户端自己 CreateCursor 一个 Cursor 资源，用
 *      ChangeWindowAttributes(CWCursor) 挂到窗口上（子窗口继承父窗口的）。
 * openbox 走的就是第 2 条（主题光标），所以它一起来指针就有了；我们这套
 * 精简桌面没有任何客户端设过光标，屏幕上看不到指针。
 *
 * 这里不依赖字体/主题：两个 depth=1 的 Pixmap 画箭头 + CreateCursor(93)。 */
/* 24x24 实心箭头（经典 left_ptr 轮廓，4x 超采样光栅化）：
 *   source（画 fore=黑）= 形状的 1 像素边界
 *   mask（决定不透明区域）   = 整个形状
 * ⇒ 边界黑、内部显示 back=白、形状外全透明。深色壁纸上也清楚。 */
#define X11_CURSOR_SIZE 24
#define X11_CURSOR_HOT 1 /* 尖端在 (1,1) */
static const uint8_t x11_arrow_src[72] = {
    0x00, 0x00, 0x00, /* y=0 */
    0x02, 0x00, 0x00, /* y=1 */
    0x06, 0x00, 0x00, /* y=2 */
    0x0a, 0x00, 0x00, /* y=3 */
    0x12, 0x00, 0x00, /* y=4 */
    0x22, 0x00, 0x00, /* y=5 */
    0xc2, 0x00, 0x00, /* y=6 */
    0x02, 0x01, 0x00, /* y=7 */
    0x02, 0x02, 0x00, /* y=8 */
    0x02, 0x04, 0x00, /* y=9 */
    0x02, 0x08, 0x00, /* y=10 */
    0x02, 0x10, 0x00, /* y=11 */
    0x02, 0x7c, 0x00, /* y=12 */
    0x02, 0x02, 0x00, /* y=13 */
    0x02, 0x02, 0x00, /* y=14 */
    0x62, 0x02, 0x00, /* y=15 */
    0x92, 0x04, 0x00, /* y=16 */
    0x8a, 0x04, 0x00, /* y=17 */
    0x06, 0x09, 0x00, /* y=18 */
    0x02, 0x09, 0x00, /* y=19 */
    0x00, 0x1a, 0x00, /* y=20 */
    0x00, 0x06, 0x00, /* y=21 */
    0x00, 0x00, 0x00, /* y=22 */
    0x00, 0x00, 0x00, /* y=23 */
};
static const uint8_t x11_arrow_mask[72] = {
    0x00, 0x00, 0x00, /* y=0 */
    0x02, 0x00, 0x00, /* y=1 */
    0x06, 0x00, 0x00, /* y=2 */
    0x0e, 0x00, 0x00, /* y=3 */
    0x1e, 0x00, 0x00, /* y=4 */
    0x3e, 0x00, 0x00, /* y=5 */
    0xfe, 0x00, 0x00, /* y=6 */
    0xfe, 0x01, 0x00, /* y=7 */
    0xfe, 0x03, 0x00, /* y=8 */
    0xfe, 0x07, 0x00, /* y=9 */
    0xfe, 0x0f, 0x00, /* y=10 */
    0xfe, 0x1f, 0x00, /* y=11 */
    0xfe, 0x7f, 0x00, /* y=12 */
    0xfe, 0x03, 0x00, /* y=13 */
    0xfe, 0x03, 0x00, /* y=14 */
    0xfe, 0x03, 0x00, /* y=15 */
    0x9e, 0x07, 0x00, /* y=16 */
    0x8e, 0x07, 0x00, /* y=17 */
    0x06, 0x0f, 0x00, /* y=18 */
    0x02, 0x0f, 0x00, /* y=19 */
    0x00, 0x1e, 0x00, /* y=20 */
    0x00, 0x06, 0x00, /* y=21 */
    0x00, 0x00, 0x00, /* y=22 */
    0x00, 0x00, 0x00, /* y=23 */
};

/* 1-bit 位图 → Pixmap。
 * 【不用 PutImage(XYBitmap)】服务器对位图每行的字节对齐有自己的一套取整
 * （实测 16 位宽的行它按 4 字节算，客户端按 (w+7)/8=2 字节发 ⇒ 长度对不上，
 * 直接回 BadLength(major=72)）。这里改成把每行拆成若干段连续 1，用
 * PolyFillRectangle 填充：协议上没有任何对齐歧义，而且位图只有 16x16、
 * 一次性画完，代价可忽略。 */
static void x11_fill_bitmap(x11_t* c, uint32_t drawable, uint32_t gc, int w,
                            int rows, const uint8_t* bits) {
  int stride = (w + 7) / 8;
  int y;
  for (y = 0; y < rows; y++) {
    int x = 0;
    const uint8_t* row = bits + y * stride;
    while (x < w) {
      int start;
      while (x < w && (row[x >> 3] & (1u << (x & 7))) == 0) {
        x++;
      }
      if (x >= w) {
        break;
      }
      start = x;
      while (x < w && (row[x >> 3] & (1u << (x & 7))) != 0) {
        x++;
      }
      x11_fill_rect(c, drawable, gc, start, y, x - start, 1);
    }
  }
}

int x11_create_pixmap_cursor(x11_t* c, uint32_t cid, uint32_t src, uint32_t mask,
                             int hot_x, int hot_y) {
  uint8_t q[32];
  memset(q, 0, sizeof(q));
  /* 【opcode 是 93，不是 94】核心协议的 CreateCursor(93) 才是"用两张 pixmap
   * 造光标"；94 是 CreateGlyphCursor（用字体字形），字段里 source/mask 被
   * 当成 Font 查 ⇒ 服务器回 BadFont(7) major=94。服务器端对照：
   * dix/tables.c:161 ProcCreateCursor / :162 ProcCreateGlyphCursor。 */
  q[0] = 93; /* CreateCursor(source/mask pixmap) */
  put16(q + 2, 8);
  put32(q + 4, cid);
  put32(q + 8, src);
  put32(q + 12, mask);
  put16(q + 16, 0x0000); /* fore：黑 */
  put16(q + 18, 0x0000);
  put16(q + 20, 0x0000);
  put16(q + 22, 0xffff); /* back：白 */
  put16(q + 24, 0xffff);
  put16(q + 26, 0xffff);
  put16(q + 28, (uint16_t)hot_x);
  put16(q + 30, (uint16_t)hot_y);
  return write_all(c->fd, q, sizeof(q));
}

/* 把 cursor 挂到窗口。子窗口在创建时继承父窗口的 cursor，所以设到 root
 * 就全局生效（之后的窗口才继承；已在的窗口要各自再设一次）。 */
int x11_set_window_cursor(x11_t* c, uint32_t win, uint32_t cursor) {
  return x11_change_attrs(c, win, 0x4000 /* CWCursor */, cursor);
}

/* 光标这套请求全是"无回复"的：发出去就完事，服务器报错（BadMatch/BadAlloc/
 * BadIDChoice…）只是一条异步 Error 包，不打 sync 就永远看不见。每几步 sync
 * 一次，把"哪一步被服务器拒绝"打到 stderr 上。 */
static int x11_cursor_step(x11_t* c, const char* what) {
  if (x11_sync(c) < 0) {
    fprintf(stderr, "x11: 光标创建失败于 %s（服务器的错误见上面的 sync got ...）\n",
            what);
    return -1;
  }
  return 0;
}

int x11_install_arrow_cursor(x11_t* c) {
  uint32_t src = x11_id(c);
  uint32_t mask = x11_id(c);
  uint32_t gc = x11_id(c);
  uint32_t cur = x11_id(c);
  if (x11_create_pixmap(c, src, 1, X11_CURSOR_SIZE, X11_CURSOR_SIZE) < 0 ||
      x11_create_pixmap(c, mask, 1, X11_CURSOR_SIZE, X11_CURSOR_SIZE) < 0 ||
      x11_create_gc(c, gc, src) < 0) {
    return -1;
  }
  if (x11_cursor_step(c, "CreatePixmap(1bpp)/CreateGC") < 0) {
    return -1;
  }
  x11_set_gc(c, gc, 0x0C /* FG|BG */, 1, 0);
  x11_fill_bitmap(c, src, gc, X11_CURSOR_SIZE, X11_CURSOR_SIZE, x11_arrow_src);
  x11_fill_bitmap(c, mask, gc, X11_CURSOR_SIZE, X11_CURSOR_SIZE,
                  x11_arrow_mask);
  if (x11_cursor_step(c, "PolyFillRectangle(1bpp pixmap)") < 0) {
    return -1;
  }
  if (x11_create_pixmap_cursor(c, cur, src, mask, X11_CURSOR_HOT,
                               X11_CURSOR_HOT) < 0) {
    return -1;
  }
  if (x11_cursor_step(c, "CreatePixmapCursor") < 0) {
    return -1;
  }
  /* pixmap 不释放：CreatePixmapCursor 之后仍作为服务器端资源留着最省事
   * （一次性四个小资源，进程生命周期内不回收到也不影响）。 */
  if (x11_set_window_cursor(c, c->root, cur) < 0) {
    return -1;
  }
  return x11_cursor_step(c, "ChangeWindowAttributes(CWCursor)");
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
  /* 用 raw：数据区（子窗口 id 列表）由本函数自己读，
   * read_reply 会先按 length 跳过一次，造成二次读取永久阻塞。 */
  if (read_reply_raw(c, rep) < 0) {
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
  if (read_reply_raw(c, rep) < 0) { /* 同上：value 由本函数自己读 */
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
  if (read_reply_raw(c, rep) < 0) { /* 同上：字符串由本函数自己读 */
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

/* ---------------- SHAPE 扩展 ---------------- */

/* QueryExtension("SHAPE")：成功返回 0 并把 major opcode 记到 c->shape_op */
int x11_shape_probe(x11_t* c) {
  static const char name[] = "SHAPE";
  uint8_t q[16];
  uint8_t rep[32];
  memset(q, 0, sizeof(q));
  c->shape_op = 0;
  q[0] = 98; /* QueryExtension：头 8 字节 + 名字（补齐到 4 字节） */
  put16(q + 2, 4);
  put16(q + 4, (uint16_t)(sizeof(name) - 1));
  memcpy(q + 8, name, sizeof(name) - 1);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return -1;
  }
  if (read_reply_raw(c, rep) < 0) {
    return -1;
  }
  if (rep[8] == 0) { /* present */
    return -1;
  }
  c->shape_op = rep[9];
  return 0;
}

/* ShapeRectangles(ShapeSet, ShapeBounding)：rects 是 n 组 {x,y,w,h}。
 * 【协议坑】扩展请求的第二个字节是扩展内 opcode（minor），不是第一个
 * 参数；写成 ShapeSet(0) 会被服务器当成"扩展内第 0 号请求"而报错
 * （BadLength/BadRequest，形状静默不生效）。头部 16 字节：
 *   opcode/minor=1/length | ShapeSet/Bounding/UnSorted/pad |
 *   window | x-offset | y-offset
 */
int x11_shape_rects(x11_t* c, uint32_t win, const int16_t* rects, int n) {
  uint8_t buf[16 + 8 * 64];
  int i;
  if (c->shape_op <= 0 || n <= 0 || n > 64) {
    return -1;
  }
  memset(buf, 0, sizeof(buf));
  buf[0] = (uint8_t)c->shape_op;
  buf[1] = 1; /* ShapeRectangles */
  put16(buf + 2, (uint16_t)((16 + 8 * n) / 4));
  buf[4] = 0; /* operation = ShapeSet */
  buf[5] = 0; /* destination-kind = ShapeBounding */
  buf[6] = 0; /* ordering = UnSorted */
  put32(buf + 8, win);
  put16(buf + 12, 0); /* x-offset */
  put16(buf + 14, 0); /* y-offset */
  for (i = 0; i < n; i++) {
    uint8_t* r = buf + 16 + i * 8;
    put16(r, (uint16_t)rects[i * 4 + 0]);
    put16(r + 2, (uint16_t)rects[i * 4 + 1]);
    put16(r + 4, (uint16_t)rects[i * 4 + 2]);
    put16(r + 6, (uint16_t)rects[i * 4 + 3]);
  }
  return write_all(c->fd, buf, 16 + 8 * n);
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
