/*******************************************************************
 * x11.c — 极简 X11 客户端协议库（实现见 x11.h 顶部说明）。
 ********************************************************************/
#include "x11.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
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

/* 【序号跟踪】所有请求都经过 write_all，靠它数出"已发出多少个请求"
 * （X11 的 sequence number）。等回复时用它区分：
 *   · 序号 < 本次请求的 Error/Reply = 之前某条请求的异步结果，必须跳过
 *     （否则会被误判成"本次请求失败"，manage() 直接放弃接管窗口）；
 *   · 序号 == 本次请求的 Error = 本条请求真失败，可以放弃等待。 */
static int g_seq;      /* 已发出的请求数（低 16 位 = 协议序列号） */
static int g_last_op;  /* 最近一条请求的 opcode（等回复超时时打出来定位） */
static int g_req_left; /* 当前请求还有多少字节没写（不含已写部分的头） */
static int g_trace;    /* X11_TRACE=1 时打印请求/回复/事件，用来抓流错位 */
/* 【异步 Error 计数】Error 包的序号属于"之前某条请求"，等回复时会被跳过继续
 * 读（不能当成当前请求失败），于是"这批请求被服务器拒了"就彻底静默了——典型
 * 例子：抢根窗口 SubstructureRedirect 被别的 WM 占着 ⇒ BadAccess，sync 却报成功。
 * 这里累加计数，x11_sync 结束时对账：自上次 sync 以来有 Error 就算失败。 */
static int g_err_count;    /* 读到的 Error 包总数 */
static int g_err_checked;  /* 上次 x11_sync 已经报过的那个值 */
/* 【明知会被拒的请求】少数请求在某些服务器/客户端组合下必然失败（典型：
 * 在"自己也选了 ButtonPress"的客户端窗口上再选 ButtonPress ⇒ BadAccess，
 * Xorg/Xephyr/Xvfb 一致）。调用方先用 x11_mute_errors(n) 静音，让这条错误
 * 既不打印也不计入 sync 对账，自己看返回值判断。 */
static int g_err_mute;
static int g_err_muted;    /* 被静音的 Error 条数 */

static void x11_trace_pkt(const char* kind, const uint8_t* p, int seq, int want) {
  if (!g_trace) {
    return;
  }
  fprintf(stderr, "[x11] %-7s type=%2d seq=%2d want=%2d | %02x %02x %02x %02x\n",
          kind, p[0], seq, want, p[0], p[1], p[2], p[3]);
}

/* 记一条服务器 Error：静音期内不打印、也不计入 x11_sync 的对账 */
static void err_note(const uint8_t* p) {
  if (g_err_mute > 0) {
    g_err_mute--;
    g_err_muted++;
    return;
  }
  g_err_count++;
  fprintf(stderr, "x11: server error code=%d major=%d minor=%d bad=%u\n", p[1],
          p[10], (int)(p[8] | (p[9] << 8)), le32(p + 4));
}

void x11_mute_errors(int n) {
  g_err_mute = n > 0 ? n : 0;
  g_err_muted = 0;
}

int x11_muted_errors(void) { return g_err_muted; }

/* 等 fd 可读/可写。EAGAIN 时【必须 poll 等】而不是 continue 自旋：
 * socket 是非阻塞的，一个 32 字节事件也可能分两次到达，自旋会把 wm 烧成
 * 100% CPU。
 *
 * 【必须切片等】把整个超时一次性交给 poll 是不行的：YiYiYa 内核的 poll
 * 唤醒偶发丢失（真机实测同一批请求里出现成串 "waiting reply timeout"，
 * 而数据其实早就在 socket 里排着了）。一次睡满 3 秒就只能干等到超时；
 * 切成 50ms 小片后，最坏只多等一片。数据已就绪时 poll 立刻返回，切片
 * 不增加任何额外往返（正常路径仍是一次 poll 就拿到）。 */
#define WAIT_SLICE_MS 50
static int wait_fd(int fd, short events, int ms) {
  int waited = 0;
  while (waited < ms) {
    struct pollfd pfd;
    int slice = ms - waited;
    int pr;
    if (slice > WAIT_SLICE_MS) {
      slice = WAIT_SLICE_MS;
    }
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    pr = poll(&pfd, 1, slice);
    if (pr > 0) {
      return 1;
    }
    if (pr < 0) {
      return 0;
    }
    waited += slice;
  }
  return 0;
}

static int write_all(int fd, const void* buf, size_t n) {
  const uint8_t* p = (const uint8_t*)buf;
  size_t off = 0;
  while (off < n) {
    ssize_t w = write(fd, p + off, n - off);
    if (w > 0) {
      off += (size_t)w;
    } else if (w < 0 && eagain()) {
      if (!wait_fd(fd, POLLOUT, 3000)) {
        return -1; /* 服务器长时间不接收：宁可报错也别把 wm 永远挂住 */
      }
    } else {
      return -1;
    }
  }
  /* 记账：一个请求 = 4 字节头（含 16 位"多少个 4 字节"）+ 其余数据。
   * 本工程的每个请求第一次 write 都包含完整头，后续 write 只有数据。 */
  if (g_req_left <= 0) {
    unsigned total = ((unsigned)p[2] | ((unsigned)p[3] << 8)) * 4u;
    g_seq = (g_seq + 1) & 0xffff;
    g_last_op = p[0];
    g_req_left = (int)total - (int)n;
    if (g_trace) {
      fprintf(stderr, "[x11] req     op=%-3d len=%u seq=%d | %02x%02x%02x%02x"
                      " %02x%02x%02x%02x %02x%02x%02x%02x\n",
              p[0], total, g_seq, p[0], p[1], p[2], p[3], n > 4 ? p[4] : 0,
              n > 5 ? p[5] : 0, n > 6 ? p[6] : 0, n > 7 ? p[7] : 0,
              n > 8 ? p[8] : 0, n > 9 ? p[9] : 0, n > 10 ? p[10] : 0,
              n > 11 ? p[11] : 0);
    }
  } else {
    g_req_left -= (int)n;
  }
  if (g_req_left < 0) {
    g_req_left = 0;
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
      if (!wait_fd(fd, POLLIN, 3000)) {
        return -1;
      }
    } else {
      return -1;
    }
  }
  return 0;
}

uint32_t x11_id(x11_t* c) { return c->id_base | (c->next_id++ & c->id_mask); }

/* 按 DISPLAY 算本地 socket 路径（":0" → /tmp/.X11-unix/X0）与 display 号。
 * 【别写死 X0】换 display 是最常见的调试动作（Xephyr :1 做嵌套桌面、Xvfb
 * 多屏对比），写死的话 wm 会悄悄连到 :0 上。只支持本机 display，TCP 不做。 */
static void x11_display_parse(char* sock, size_t sc, char* num, size_t nc) {
  const char* d = getenv("DISPLAY");
  const char* colon;
  int n = 0;
  if (d != NULL && (colon = strrchr(d, ':')) != NULL) {
    n = atoi(colon + 1);
    if (n < 0) {
      n = 0;
    }
  }
  snprintf(sock, sc, "/tmp/.X11-unix/X%d", n);
  snprintf(num, nc, "%d", n);
}

static unsigned be16(const uint8_t* p) { return (unsigned)((p[0] << 8) | p[1]); }

/* "WxH+X+Y" → 四个数（宽高必须给，偏移可省且可为负）。分隔符（'x'、'+'、'-'
 * 之外的任何字符）一律跳过，所以 "200x160"、"240x180+80+60"、"300x200-10-10"
 * 都能吃。第一个数前若有符号按正数处理，免得 "x" 被当成负数号。 */
int x11_parse_geom(const char* s, int* w, int* h, int* x, int* y) {
  int v[4] = {0, 0, 0, 0};
  int n = 0;
  const char* p = s;
  if (s == NULL) {
    return -1;
  }
  while (*p != '\0' && n < 4) {
    int sign = 1;
    int digits = 0;
    int val = 0;
    if (*p == '-' && n > 0) {
      sign = -1;
      p++;
    } else if (*p == '+') {
      p++;
    }
    while (*p >= '0' && *p <= '9') {
      val = val * 10 + (*p - '0');
      p++;
      digits = 1;
    }
    if (!digits) {
      p++; /* 不是数字（'x' 或多写的一个 '+'）：跳过一个字符继续 */
      continue;
    }
    v[n++] = sign * val;
  }
  if (n < 2 || v[0] <= 0 || v[1] <= 0) {
    return -1;
  }
  if (w != NULL) {
    *w = v[0];
  }
  if (h != NULL) {
    *h = v[1];
  }
  if (x != NULL) {
    *x = (n > 2) ? v[2] : 0;
  }
  if (y != NULL) {
    *y = (n > 3) ? v[3] : 0;
  }
  return 0;
}

static int read_n(FILE* f, void* p, unsigned n) {
  return n == 0 || fread(p, 1, n, f) == n;
}

/* 从 Xauthority 里取 MIT-MAGIC-COOKIE-1。XQuartz 这类 server 要求鉴权，
 * 不带 cookie 的 setup 会被直接拒掉（"setup rejected (status 0)"）。
 * 文件格式：每条目 = family(2) addrlen(2) addr numlen(2) num namelen(2) name
 * datalen(2) data，长度字段一律大端 16 位。取第一条 name 匹配、display 号
 * 匹配（number 为空＝任意屏）的。成功返回 cookie 长度，失败 -1。 */
static int x11_read_cookie(const char* file, const char* num, uint8_t* out,
                           int cap) {
  FILE* f = fopen(file, "rb");
  uint8_t hdr[2], addr[256], nbuf[64], name[128], data[256];
  if (f == NULL) {
    return -1;
  }
  for (;;) {
    unsigned fam, alen, nlen, namelen, dlen;
    if (!read_n(f, hdr, 2)) {
      break;
    }
    fam = be16(hdr);
    if (!read_n(f, hdr, 2)) {
      break;
    }
    alen = be16(hdr);
    if (alen > sizeof(addr) || !read_n(f, addr, alen)) {
      break;
    }
    if (!read_n(f, hdr, 2)) {
      break;
    }
    nlen = be16(hdr);
    if (nlen > sizeof(nbuf) || !read_n(f, nbuf, nlen)) {
      break;
    }
    if (!read_n(f, hdr, 2)) {
      break;
    }
    namelen = be16(hdr);
    if (namelen > sizeof(name) || !read_n(f, name, namelen)) {
      break;
    }
    if (!read_n(f, hdr, 2)) {
      break;
    }
    dlen = be16(hdr);
    if (dlen > sizeof(data) || !read_n(f, data, dlen)) {
      break;
    }
    if (namelen == 18 && memcmp(name, "MIT-MAGIC-COOKIE-1", 18) == 0 &&
        (nlen == 0 || (strlen(num) == nlen && memcmp(nbuf, num, nlen) == 0))) {
      (void)fam;
      if ((int)dlen > cap) {
        dlen = (unsigned)cap;
      }
      memcpy(out, data, dlen);
      fclose(f);
      return (int)dlen;
    }
  }
  fclose(f);
  return -1;
}

static int x11_connect_once(x11_t* c, const char* path);

/* 【为什么要重试】server 刚起来时 socket 已经能 connect，但 setup 响应要等
 * 一会儿（Xephyr/Xvfb 初始化、服务器正忙于整屏重绘也见过）。一次失败就把
 * 客户端判死，开机自启的 wall/panel/dock 会随机缺一个，日志只剩"读 setup
 * 响应失败"，很难往这里想。这里最多试 8 次、每次间隔 120ms。 */
int x11_connect(x11_t* c, const char* path) {
  int i;
  for (i = 0; i < 8; i++) {
    if (x11_connect_once(c, path) == 0) {
      return 0;
    }
    if (c->fd > 0) {
      close(c->fd);
      c->fd = -1;
    }
    usleep(120 * 1000);
  }
  return -1;
}

static int x11_connect_once(x11_t* c, const char* path) {
  struct sockaddr_un sa;
  char disp[64];
  char dnum[16] = "0";
  uint8_t setup[128];
  uint8_t hdr[8];
  static uint8_t body[8192];
  unsigned extra, vendor_len, nformats, off;

  memset(c, 0, sizeof(*c));
  g_trace = (getenv("X11_TRACE") != NULL);
  g_seq = 0;
  g_req_left = 0;
  g_err_count = 0;
  g_err_checked = 0;
  if (path == NULL) {
    x11_display_parse(disp, sizeof(disp), dnum, sizeof(dnum));
    path = disp;
  } else {
    const char* x = strrchr(path, 'X'); /* 显式路径：display 号取尾部的 X<n> */
    if (x != NULL && x[1] != '\0') {
      snprintf(dnum, sizeof(dnum), "%d", atoi(x + 1));
    }
  }
  c->fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (c->fd < 0) {
    perror("x11: socket");
    return -1;
  }
  memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", path);
  {
    /* 【地址长度按 offsetof 算】按"sizeof(sun_family) + 路径长度"在 Linux
     * 上等价，但在 BSD/macOS 上 sun_family 只有 1 字节、前面还有 sun_len，
     * 长度算错 connect 直接 ENOENT。 */
    size_t alen = offsetof(struct sockaddr_un, sun_path) + strlen(sa.sun_path);
#ifdef __APPLE__
    sa.sun_len = (unsigned char)alen;
#endif
    if (connect(c->fd, (struct sockaddr*)&sa, (socklen_t)alen) < 0) {
      perror("x11: connect");
      close(c->fd);
      c->fd = -1;
      return -1;
    }
  }
  /* 【必须非阻塞】X11 是双向管道：服务器往这条连接写事件的同时，我们也在
   * 往里写请求。阻塞 socket 下只要一方缓冲满就互相等死 —— 窗口一多，服务器
   * 要写的 StructureNotify/Expose 事件量上来，我们（正忙于处理事件）不读，
   * 它就写不出去；它写不出去，我们下一次 write_all 就卡在 write() 上，
   * 整个 wm 冻住、所有窗口不再刷新（体感：开几个应用后点哪都不动）。
   * read_exact/write_all 里本来就有 EAGAIN 分支，这里把 socket 真正设成
   * 非阻塞，让那两条分支生效。 */
  {
    int fl = fcntl(c->fd, F_GETFL, 0);
    if (fl >= 0) {
      fcntl(c->fd, F_SETFL, fl | O_NONBLOCK);
    }
  }
  /* 连接建立：'l' + 协议 11.0 + 鉴权（Xauthority 里有 cookie 就带）。
   * Xvfb/Xephyr 不校验，cookie 取不到就按无鉴权发（25 年来的老姿势）。 */
  {
    static const char aname[] = "MIT-MAGIC-COOKIE-1";
    char defauth[512];
    uint8_t cookie[64];
    const char* xa = getenv("XAUTHORITY");
    unsigned nlen = 0, clen = 0, pad_n, pad_d, total;
    int got;
    if (xa == NULL || xa[0] == '\0') {
      const char* home = getenv("HOME");
      snprintf(defauth, sizeof(defauth), "%s/.Xauthority",
               home != NULL ? home : "");
      xa = defauth;
    }
    got = x11_read_cookie(xa, dnum, cookie, (int)sizeof(cookie));
    if (got > 0) {
      clen = (unsigned)got;
      nlen = (unsigned)(sizeof(aname) - 1);
    }
    pad_n = (4u - (nlen & 3u)) & 3u;
    pad_d = (4u - (clen & 3u)) & 3u;
    total = 12u + nlen + pad_n + clen + pad_d;
    if (total > sizeof(setup)) {
      return -1;
    }
    memset(setup, 0, total);
    setup[0] = 'l';
    put16(setup + 2, 11); /* protocol-major=11, minor=0 */
    put16(setup + 6, (uint16_t)nlen);
    put16(setup + 8, (uint16_t)clen);
    memcpy(setup + 12, aname, nlen);
    memcpy(setup + 12 + nlen + pad_n, cookie, clen);
    if (write_all(c->fd, setup, total) < 0) {
      return -1;
    }
  }
  if (read_exact(c->fd, hdr, sizeof(hdr)) < 0) {
    /* 【别静默失败】server 还在初始化（刚起的 Xephyr/Xvfb）时 socket 已经能
     * connect，但 setup 响应要等一会儿才来；读失败（含 3 秒超时）就走这里。
     * 以前这里直接 return，调用方看日志一片空白，很像"wm 一启动就没了"。 */
    fprintf(stderr, "x11: 读 setup 响应失败（server 没就绪？socket=%s）\n", path);
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
  /* setup 那 12 字节不是 X 请求，序列号从第一条真请求重新数 */
  g_seq = 0;
  g_req_left = 0;
  return 0;
}

void x11_close(x11_t* c) {
  if (c != NULL && c->fd >= 0) {
    close(c->fd);
    c->fd = -1;
  }
}

#define X11_EVQ_SLOTS ((int)(sizeof(((x11_t*)0)->evq) / 32))

static int evq_push(x11_t* c, const uint8_t* ev) {
  static int warned;
  int next = (c->evq_tail + 1) % X11_EVQ_SLOTS;
  if (next == c->evq_head) {
    /* 【别静默丢】队列满说明我们处理事件的速度跟不上服务器产出（窗口多 +
     * 拖动时 Expose/Configure 洪峰）。丢掉的多是 Expose/Notify，丢一条
     * MapRequest 就等于"客户端窗口永远不出现"。打一次日志方便定位。 */
    if (!warned) {
      warned = 1;
      fprintf(stderr, "x11: event queue full (%d slots), dropping (type=%d)\n",
              X11_EVQ_SLOTS - 1, ev[0]);
    }
    return -1;
  }
  memcpy(c->evq + c->evq_tail * 32, ev, 32);
  c->evq_tail = next;
  return 0;
}

static int evq_pop(x11_t* c, uint8_t* ev) {
  if (c->evq_head == c->evq_tail) {
    return 0;
  }
  memcpy(ev, c->evq + c->evq_head * 32, 32);
  c->evq_head = (c->evq_head + 1) % X11_EVQ_SLOTS;
  return 1;
}

/* 读 32 字节回复，不碰"附加数据"字段。
 * 【坑 1】定长回复（GetInputFocus/InternAtom 等）里 offset 4 的 length 字段
 * Xorg 会填 1（不是 0，也不是附加字节数），照着它去读 4 字节会永远等不到
 * 数据 ⇒ wm/wall 卡死在 sync 上。定长回复一律用这个。
 * 【坑 2·更致命】回复和事件在同一条流里。服务器处理完我们批发的请求后，
 * ReparentNotify / UnmapNotify / Expose / ButtonPress 会排在回复前面到达。
 * 直接读 32 字节当回复 = 拿事件头当回复字段（尺寸、属性全变垃圾），而且
 * 真正的回复还留在流里，之后每一次读都错位一格。这里必须：事件 → 入队，
 * 异步 Error → 打印后跳过，只有真 Reply 才返回。 */
static int read_reply_raw(x11_t* c, uint8_t* out32) {
  uint8_t rep[32];
  /* 刚发出去的那条请求的序号 = 我们在等的回复序号（调用方都是"发完立刻等"） */
  int want = g_seq;
  for (;;) {
    int seq;
    /* 【必须有超时】没有它的话，某个请求拿不到回复时客户端就永久卡在
     * read 上：既不再读事件，也让服务器写这个 socket 写满后一起卡死
     * （表现为整屏光标不动）。3 秒对本地 unix socket 已经是天文数字。
     * 用 wait_fd（切片等），内核丢唤醒时最多多等一片 50ms 而不是 3 秒。 */
    if (!wait_fd(c->fd, POLLIN, 3000)) {
      /* 【带上 pid 和 opcode】同一块控制台上好几个客户端都在打印，只有
       * seq 根本分不清是谁、卡在哪条请求上（曾经为此反复猜）。 */
      fprintf(stderr, "x11: reply timeout (pid=%d seq=%d op=%d)\n", (int)getpid(),
              want, g_last_op);
      return -1;
    }
    if (read_exact(c->fd, rep, sizeof(rep)) < 0) {
      return -1;
    }
    seq = rep[2] | (rep[3] << 8);
    x11_trace_pkt(rep[0] == 1 ? "reply" : (rep[0] == 0 ? "error" : "event"),
                  rep, seq, want);
    if (rep[0] == 1) { /* Reply */
      if (seq != want) {
        continue; /* 陈旧回复：属于早先被放弃的请求 */
      }
      if (out32 != NULL) {
        memcpy(out32, rep, sizeof(rep));
      }
      return 0;
    }
    if (rep[0] == 0) { /* Error */
      err_note(rep);
      if (seq != want) {
        continue; /* 老请求的异步错误：跳过，别当成"本次失败"（计数留给 sync 对账） */
      }
      return -2; /* 本条请求被拒：不会再有 Reply，必须立刻放弃等待 */
    }
    evq_push(c, rep); /* 事件：交给主循环 */
  }
}

/* 丢掉 n 字节（回复里调用方不关心的附加数据 / 4 字节对齐填充）。
 * 【必须精确丢完】X11 是裸字节流：少读或多读一个字节，后面所有事件与
 * 回复都会错位，表现为"服务器回了个莫名其妙的东西"+ 客户端拿垃圾 id
 * 继续发请求。 */
static int skip_bytes(x11_t* c, unsigned n) {
  uint8_t junk[64];
  while (n > 0) {
    unsigned k = n > (unsigned)sizeof(junk) ? (unsigned)sizeof(junk) : n;
    if (read_exact(c->fd, junk, k) < 0) {
      return -1;
    }
    n -= k;
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
    g_err_checked = g_err_count;
    fprintf(stderr, "x11: sync got type=%d code=%d major=%d minor=%d bad=%u\n",
            rep[0], rep[1], rep[10], (int)(rep[8] | (rep[9] << 8)),
            le32(rep + 4));
    return -1;
  }
  /* 最后一关：read_reply_raw 会把"老序号"的 Error 跳过继续等 Reply（它不能
   * 把别的请求的失败算到当前请求头上），但同步点本身就是个错误检查点——
   * 自上次 sync 起只要服务器发过 Error，这批请求里就有被拒的，必须报失败，
   * 否则调用方会以为"抢根窗口/建 frame"都成功了。 */
  if (g_err_count != g_err_checked) {
    fprintf(stderr, "x11: 自上次同步以来有 %d 条请求被服务器拒绝\n",
            g_err_count - g_err_checked);
    g_err_checked = g_err_count;
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
  int line = w * 4; /* 每行字节数（32bpp，w*4 天然 4 字节对齐） */
  /* 【请求长度字段只有 16 位】核心协议的 length 是"多少个 4 字节"，最大
   * 65535 ⇒ 单请求数据上限 65535*4-24 = 262116 字节。超过就溢出成小数
   * （实测 256x256 图 length 算成 6），服务器只读走 24 字节头、把真正
   * 的图像数据当成后续请求解析 ⇒ 服务器端请求流错位、内存被踩。
   * 按整行切成多请求，每个请求的 dst-y 顺延。 */
  int max_rows = line > 0 ? (int)((65535u * 4u - 24u) / (unsigned)line) : 0;
  int y = 0;

  if (w <= 0 || rows <= 0 || line <= 0) {
    return -1;
  }
  if (max_rows < 1) {
    return -1;
  }
  while (y < rows) {
    int n = rows - y;
    uint32_t bytes;
    if (n > max_rows) {
      n = max_rows;
    }
    bytes = (uint32_t)line * (uint32_t)n;
    memset(q, 0, sizeof(q));
    q[0] = 72;
    q[1] = 2; /* ZPixmap */
    put16(q + 2, (uint16_t)((sizeof(q) + bytes) / 4));
    put32(q + 4, drawable);
    put32(q + 8, gc);
    put16(q + 12, (uint16_t)w);
    put16(q + 14, (uint16_t)n);
    put16(q + 16, (uint16_t)dst_x);
    put16(q + 18, (uint16_t)(dst_y + y));
    q[20] = 0;
    q[21] = (uint8_t)c->root_depth;
    if (write_all(c->fd, q, sizeof(q)) < 0) {
      return -1;
    }
    if (write_all(c->fd, bgrx + (size_t)y * (size_t)line, bytes) < 0) {
      return -1;
    }
    y += n;
  }
  return 0;
}

/* 填充圆弧：PolyFillArc(71)。一条弧 12 字节（x,y,w,h,angle1,angle2），
 * 全是 Int16；角度单位 1/64 度、逆时针（X 的 y 轴向下，所以视觉上是逆时针）。 */
int x11_fill_arc(x11_t* c, uint32_t drawable, uint32_t gc, int x, int y,
                 int w, int h, int angle1, int angle2) {
  uint8_t q[24];
  if (w <= 0 || h <= 0) {
    return 0;
  }
  memset(q, 0, sizeof(q));
  q[0] = 71; /* PolyFillArc */
  put16(q + 2, 6); /* 3 个字头 + 1 条弧 3 个字 */
  put32(q + 4, drawable);
  put32(q + 8, gc);
  put16(q + 12, (uint16_t)x);
  put16(q + 14, (uint16_t)y);
  put16(q + 16, (uint16_t)w);
  put16(q + 18, (uint16_t)h);
  put16(q + 20, (uint16_t)angle1);
  put16(q + 22, (uint16_t)angle2);
  return write_all(c->fd, q, sizeof(q));
}

int x11_fill_rect(x11_t* c, uint32_t drawable, uint32_t gc, int x, int y,
                  int w, int h) {
  uint8_t q[20];
  /* 【空矩形不发】宽或高 <= 0 时服务器要把一个退化矩形并进 region，
   * debug 构建的 pixman 会断言刷屏（也纯属浪费一次请求）。 */
  if (w <= 0 || h <= 0) {
    return 0;
  }
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

/* 选事件掩码，被服务器拒（BadAccess）时返回 -1。为什么要这个：在"自己也选了
 * ButtonPress"的客户端窗口上再选 ButtonPress，Xorg/Xephyr/Xvfb 一致回
 * BadAccess —— 而这条错误是异步的，不 sync 一次根本不知道成败。静音是为了
 * 不让可预期的失败刷屏，也不污染 x11_sync 的错误对账。 */
int x11_select_input_optional(x11_t* c, uint32_t win, uint32_t event_mask) {
  int muted;
  x11_mute_errors(1);
  if (x11_select_input(c, win, event_mask) < 0 || x11_sync(c) < 0) {
    x11_mute_errors(0);
    return -1;
  }
  muted = x11_muted_errors();
  x11_mute_errors(0);
  return muted > 0 ? -1 : 0;
}

uint32_t x11_create_window(x11_t* c, int x, int y, int w, int h, uint32_t bg,
                           int override_redirect, uint32_t event_mask) {
  /* 【必须 44 字节】CreateWindow 头 32 + 最多 3 个 value(12)：back-pixel /
   * override-redirect / event-mask。原来写成 q[40]，第三个 value 落在
   * q+40 越界，write_all 又按 44 字节发出去 ⇒ 发出的是栈垃圾当 event_mask。 */
  uint8_t q[44];
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
  unsigned pad;
  /* 【空串不发】len<=0 的 ImageText8 让服务器去画一个零宽文本，debug 版
   * pixman 会刷 "r->x1 < r->x2 was false"（退化的 region 矩形），而且净是
   * 白跑一趟。标题为空/窗口窄到一个字都放不下时就会走到这里。 */
  if (len <= 0 || s == NULL) {
    return 0;
  }
  pad = (unsigned)((4 - (len & 3)) & 3);
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
  uint8_t rep[32];
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
  /* 【GrabPointer 是有回复的请求】status: 0=Success 1=AlreadyGrabbed
   * 2=InvalidTime 3=NotViewable 4=Frozen。以前没读这条回复：reply 留在流里
   * 凑巧会被"下一包是 Reply 就丢掉"的逻辑吃掉，看起来无害，但**拿不到失败
   * 原因**——grab 失败时 MotionNotify 会直接送给指针下的客户端（xeyes 会跟着
   * 鼠标每像素重绘整个眼睛），表现为"窗口拖不动 + 越拖越卡"。 */
  if (read_reply_raw(c, rep) < 0) {
    return -1;
  }
  if (rep[1] != 0) {
    fprintf(stderr, "x11: GrabPointer 失败 status=%d win=0x%x mask=0x%x\n",
            rep[1], win, event_mask);
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

/* 设置标题：wm 接管窗口时读 WM_NAME 缓存进标题栏（只读一次，之后不 round-trip） */
int x11_set_title(x11_t* c, uint32_t win, const char* title) {
  return x11_set_property_str(c, win, x11_intern_atom(c, "WM_NAME"), title);
}

/* 声明自己支持 WM_DELETE_WINDOW。
 * 【两个 atom 都要，别只比一个】X 的关闭请求是一个 ClientMessage：
 *     type 字段 = WM_PROTOCOLS，data[0] = WM_DELETE_WINDOW
 * 只拿 WM_DELETE_WINDOW 一个 atom 去比对 type 字段永远不相等（踩过：点 ×
 * 之后客户端不退出，wm 只能把它 XKillClient 掉，窗口没了进程还挂着）。
 * 成功返回 0，*type_atom / *data_atom 分别收这两个 atom。 */
int x11_enable_delete_window(x11_t* c, uint32_t win, uint32_t* type_atom,
                             uint32_t* data_atom) {
  uint32_t protos = x11_intern_atom(c, "WM_PROTOCOLS");
  uint32_t del = x11_intern_atom(c, "WM_DELETE_WINDOW");
  if (protos == 0 || del == 0) {
    return -1;
  }
  if (x11_set_property32(c, win, protos, X11_ATOM_ATOM, &del, 1) < 0) {
    return -1;
  }
  if (type_atom != NULL) {
    *type_atom = protos;
  }
  if (data_atom != NULL) {
    *data_atom = del;
  }
  return 0;
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

/* 一次配置 x/y/w/h：mask 按 X/Y/W/H 位序给值，传 X11_KEEP 表示该项不改。
 * 值一律按 32 位小端写：x/y 负数是合法的（服务器按 INT16 解释 value）。 */
int x11_configure(x11_t* c, uint32_t win, int x, int y, int w, int h) {
  uint8_t q[28];
  uint32_t mask = 0;
  unsigned n = 0;
  /* 【宽高为 0 的请求不下发】X 协议允许 0 尺寸窗口，但服务器内部要拿它去拼
   * region，遇到退化矩形 debug 版 pixman 就刷 "y1 < y2 was false"。窗口宽度
   * 0 对用户也没意义 —— 当"不改"处理。
   * 【flag 必须先算好】mask 与真正写入的值个数必须严格对应：少写一个值服务器
   * 会把后面的字节当新请求头，多写一个它会按下一个请求解析 —— 两种都是整条
   * 请求流错位，比退化矩形严重得多。 */
  int sx = (x != X11_KEEP);
  int sy = (y != X11_KEEP);
  int sw = (w != X11_KEEP && w > 0);
  int sh = (h != X11_KEEP && h > 0);
  memset(q, 0, sizeof(q));
  q[0] = 12; /* ConfigureWindow */
  put32(q + 4, win);
  if (sx) {
    mask |= 0x01;
  }
  if (sy) {
    mask |= 0x02;
  }
  if (sw) {
    mask |= 0x04;
  }
  if (sh) {
    mask |= 0x08;
  }
  put16(q + 8, (uint16_t)mask);
  if (sx) {
    put32(q + 12 + n * 4, (uint32_t)x);
    n++;
  }
  if (sy) {
    put32(q + 12 + n * 4, (uint32_t)y);
    n++;
  }
  if (sw) {
    put32(q + 12 + n * 4, (uint32_t)w);
    n++;
  }
  if (sh) {
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
  /* 【opcode 是 6，不是 85】85 是 AllocNamedColor（参数是 colormap + 颜色名，
   * 而且有 reply）：发过去服务器按"颜色名长度 0"解析、回 BadLength/Reply，
   * 客户端又不读这条回复 ⇒ 回复字节留在流里把后续事件全部错位。 */
  q[0] = 6; /* ChangeSaveSet */
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
  /* 【超出的必须读完再丢】只读 max 个、剩下 n-max 个留在流里会让后面
   * 每个事件/回复都错位（root 子窗口超过 max 时必现）。 */
  for (i = 0; i < n; i++) {
    uint8_t v[4];
    if (read_exact(c->fd, v, 4) < 0) {
      return i < max ? i : max;
    }
    if (i < max) {
      out[i] = le32(v);
    }
  }
  return n > max ? max : n;
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
  /* 【定长回复一律 raw】GetGeometry 没有任何附加数据；按 length 字段去
   * 跳字节，一旦服务器在该字段填了非 0（Xorg 的定长回复确实会填 1），
   * 就会多吃 4 字节，之后整条流的对齐全错。 */
  if (read_reply_raw(c, rep) < 0) {
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

int x11_query_pointer(x11_t* c, uint32_t win, int* x, int* y, int* wx, int* wy,
                      uint32_t* mask) {
  uint8_t q[8];
  uint8_t rep[32];
  memset(q, 0, sizeof(q));
  q[0] = 38; /* QueryPointer */
  put16(q + 2, 2);
  put32(q + 4, win);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return -1;
  }
  /* 定长回复（32 字节，没有附加数据）：同 GetGeometry，必须用 raw 读 */
  if (read_reply_raw(c, rep) < 0) {
    return -1;
  }
  if (rep[1] == 0) { /* same-screen = false：指针在别的屏上，坐标无意义 */
    return -1;
  }
  if (x) {
    *x = (int16_t)le16(rep + 16);
  }
  if (y) {
    *y = (int16_t)le16(rep + 18);
  }
  if (wx) {
    *wx = (int16_t)le16(rep + 20);
  }
  if (wy) {
    *wy = (int16_t)le16(rep + 22);
  }
  if (mask) {
    *mask = (uint32_t)le16(rep + 24);
  }
  return 0;
}

int x11_get_attrs(x11_t* c, uint32_t win, int* map_state, int* override_redirect,
                  uint32_t* all_event_masks) {
  uint8_t q[8];
  /* 【回复是 44 字节，不是 32】GetWindowAttributes 的 length 字段是 3（多余
   * 3 个字），前 32 字节到 colormap 为止，后面还有 all-event-masks(4) /
   * your-event-masks(4) / do-not-propagate-mask(2) / pad(2)。只读 32 字节会把
   * 这 12 个字节留在流里 ⇒ 下一个包从错位处解析，表现为"服务器回了个
   * code=0 major=0 bad=0 的错误包"（其实是全零的事件掩码被当成了包头）。 */
  uint8_t rep[44];
  memset(q, 0, sizeof(q));
  q[0] = 3; /* GetWindowAttributes */
  put16(q + 2, 2);
  put32(q + 4, win);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return -1;
  }
  if (read_reply_raw(c, rep) < 0) {
    return -1;
  }
  if (read_exact(c->fd, rep + 32, 12) < 0) { /* 尾部 3 个字必须读走 */
    return -1;
  }
  if (map_state) {
    *map_state = rep[26]; /* map-state */
  }
  if (override_redirect) {
    *override_redirect = rep[27];
  }
  if (all_event_masks) {
    /* 偏移 28 是 colormap，all-event-masks 在后 32 字节区 */
    *all_event_masks = le32(rep + 32);
  }
  return 0;
}

/* 读 32 位格式属性（返回元素个数；0 = 没有/类型不符） */
int x11_get_property32(x11_t* c, uint32_t win, uint32_t prop, uint32_t* out,
                       int max) {
  uint8_t q[24];
  uint8_t rep[32];
  int n32, i;
  uint32_t total;
  memset(q, 0, sizeof(q));
  q[0] = 20; /* GetProperty */
  put16(q + 2, 6);
  put32(q + 4, win);
  put32(q + 8, prop);
  put32(q + 12, 0); /* AnyPropertyType */
  put32(q + 16, 0); /* long offset */
  put32(q + 20, (uint32_t)max);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return -1;
  }
  if (read_reply_raw(c, rep) < 0) { /* 同上：value 由本函数自己读 */
    return -1;
  }
  /* 【按 length 字段把数据区读完】value 的字节数由回复头的 length 决定
   * （4 字节单位），不是 nItems*format/8；少读的部分会留在流里，之后
   * 每个事件、每次回复读取都错位。 */
  total = le32(rep + 4) * 4u;
  if (rep[1] != 32 || le32(rep + 16) == 0) {
    skip_bytes(c, total);
    return 0; /* 属性不存在/不是 32 位：确实 0 项 */
  }
  n32 = (int)le32(rep + 16);
  if (n32 > max) {
    n32 = max;
  }
  for (i = 0; i < n32; i++) {
    uint8_t v[4];
    if (read_exact(c->fd, v, 4) < 0) {
      return -1;
    }
    out[i] = le32(v);
  }
  if (total > (uint32_t)n32 * 4u && skip_bytes(c, total - (uint32_t)n32 * 4u) < 0) {
    return -1;
  }
  /* 【失败必须返回 -1，不能和"读到 0 项"混在一起】调用方（dock）拿 0 当
   * "没有运行窗口"，一次读失败就会把胶囊缩到最小、下次成功又长回来，
   * 看上去就是"dock 一直在刷新"。 */
  return n32;
}

/* 写字符串属性（ChangeProperty，format=8，type=STRING）。自研客户端靠它
 * 设 WM_NAME，wm 接管时读一次拿到的就是标题栏文字（否则显示 "(no name)"）。
 * 【必须补 4 字节对齐】协议里 length 按 4 字节字算，数据不满一个字时服务器
 * 会把紧随其后的请求头当属性值吃掉 ⇒ 之后整条流错位。 */
int x11_set_property_str(x11_t* c, uint32_t win, uint32_t prop, const char* s) {
  uint8_t q[24 + 256];
  int n = (int)strlen(s);
  int padded;
  if (n > 250) {
    n = 250;
  }
  padded = (n + 3) & ~3;
  memset(q, 0, (size_t)(24 + padded));
  q[0] = 18; /* ChangeProperty */
  q[1] = 0;  /* mode = Replace */
  put16(q + 2, (uint16_t)((24 + padded) / 4));
  put32(q + 4, win);
  put32(q + 8, prop);
  put32(q + 12, X11_ATOM_STRING);
  q[16] = 8; /* format */
  put32(q + 20, (uint32_t)n);
  memcpy(q + 24, s, (size_t)n);
  return write_all(c->fd, q, (size_t)(24 + padded));
}

/* 读字符串属性（WM_NAME 之类，format=8），返回字节数 */
int x11_get_property_str(x11_t* c, uint32_t win, uint32_t prop, char* out,
                         int max) {
  uint8_t q[24];
  uint8_t rep[32];
  int nbytes;
  int n;
  int i;
  uint32_t total;
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
  /* 【format=8 的 value 会补齐到 4 字节】例如 WM_NAME="xeyes"（5 字节）
   * 服务器发 8 字节（length=2 个 4 字节字）。只读 nbytes=5 会让后面 3 个
   * 填充字节留在流里 ⇒ 之后读到的事件/回复全部错位（wm 拿到垃圾 window
   * id 继续发请求，服务器报一堆 BadWindow/BadDrawable）。这里按回复头的
   * length 把整个数据区读干净。 */
  total = le32(rep + 4) * 4u;
  if (rep[1] != 8) {
    skip_bytes(c, total);
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
  if (total > (uint32_t)nbytes) {
    skip_bytes(c, total - (uint32_t)nbytes); /* 4 字节对齐的填充 */
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
  int i, m = 0;
  if (c->shape_op <= 0 || n <= 0) {
    return -1;
  }
  memset(buf, 0, sizeof(buf));
  buf[4] = 0; /* operation = ShapeSet */
  buf[5] = 0; /* destination-kind = ShapeBounding */
  buf[6] = 0; /* ordering = UnSorted */
  put32(buf + 8, win);
  put16(buf + 12, 0); /* x-offset */
  put16(buf + 14, 0); /* y-offset */
  /* 【丢掉退化矩形】宽或高 <= 0 的矩形并进 region 会让服务器断言
   * （pixman_region_append_non_o: r->x1 < r->x2 / y1 < y2 was false）。 */
  for (i = 0; i < n && m < 64; i++) {
    int16_t x1 = rects[i * 4 + 0];
    int16_t y1 = rects[i * 4 + 1];
    int16_t w = rects[i * 4 + 2];
    int16_t h = rects[i * 4 + 3];
    uint8_t* r;
    if (w <= 0 || h <= 0) {
      continue;
    }
    r = buf + 16 + m * 8;
    put16(r, (uint16_t)x1);
    put16(r + 2, (uint16_t)y1);
    put16(r + 4, (uint16_t)w);
    put16(r + 6, (uint16_t)h);
    m++;
  }
  if (m == 0) {
    return -1; /* 全是空矩形：不发（空形状会让窗口彻底不可见/不可点） */
  }
  buf[0] = (uint8_t)c->shape_op;
  buf[1] = 1; /* ShapeRectangles */
  put16(buf + 2, (uint16_t)((16 + 8 * m) / 4));
  return write_all(c->fd, buf, 16 + 8 * m);
}

int x11_get_keyboard_mapping(x11_t* c, unsigned first_keycode, unsigned count,
                             uint32_t* keysyms, int max) {
  uint8_t q[8];
  uint8_t rep[32];
  unsigned per, want, i, got;
  uint32_t total;
  memset(q, 0, sizeof(q));
  q[0] = 101; /* GetKeyboardMapping：keycode → keysym 表 */
  q[4] = (uint8_t)first_keycode;
  q[5] = (uint8_t)count;
  put16(q + 2, 2);
  if (write_all(c->fd, q, sizeof(q)) < 0) {
    return -1;
  }
  if (read_reply_raw(c, rep) < 0) {
    return -1;
  }
  per = rep[1]; /* 每个 keycode 几个 keysym（Xorg 常见 2，shift 切换第二列） */
  total = le32(rep + 4) * 4u; /* 数据区字节数（4 字节单位），必须读干净 */
  if (per == 0 || total == 0) {
    if (total > 0) {
      skip_bytes(c, total);
    }
    return 0;
  }
  want = per * count;
  got = (want > (unsigned)max) ? (unsigned)max : want;
  for (i = 0; i < got; i++) {
    uint8_t v[4];
    if (read_exact(c->fd, v, 4) < 0) {
      return -1;
    }
    keysyms[i] = le32(v);
  }
  if (total > got * 4u && skip_bytes(c, total - got * 4u) < 0) {
    return -1;
  }
  return (int)per;
}

int x11_next_event(x11_t* c, uint8_t* ev) {
  for (;;) {
    ssize_t r;
    /* 排队的优先：它们是在等某个回复时先到的事件，顺序必须保持 */
    if (evq_pop(c, ev)) {
      return 1;
    }
    r = read(c->fd, ev, 32);
    if (r == 0) {
      return -1;
    }
    if (r < 0) {
      return eagain() ? 0 : -1;
    }
    if (r < 32 && read_exact(c->fd, ev + r, (size_t)(32 - r)) < 0) {
      return -1;
    }
    /* 主循环读路径也要打点：早先只有"等回复时顺带读到的事件"才进 trace，
     * 正常事件一个都看不到，排查 dock/wm 之间的事件到底是没发出去还是没被
     * 认出来时白绕了一大圈。 */
    x11_trace_pkt("event", ev, ev[2] | (ev[3] << 8), -1);
    /* 这里不该出现 Reply（没有请求在等），Error 是异步错误：都丢掉重读，
     * 别把它们的字节当事件字段（type=0 会让 handle 走 default 分支，
     * type=1 更会撞上"CreateWindow"之类的编号）。 */
    if (ev[0] == 0 || ev[0] == 1) {
      if (ev[0] == 0) {
        err_note(ev);
      }
      continue;
    }
    return 1;
  }
}
