/*
 * test-vnc.c —— 不依赖 HDMI 的最小 VNC 桌面。
 *
 * Pi 5 还没有显示驱动。画面画在本进程的缓冲里，按 RFB 3.8（Raw）从
 * 5900 端口送出。Mac 上：
 *   open vnc://192.168.1.250
 *
 * 协议栈已经占着 /dev/net。不要同时跑 test-net。可以和 test-http 一起跑，
 * 它占的是 80。阻塞的 accept/read/write 在没数据时会返回 EAGAIN，这里接着等。
 *
 * macOS 屏幕共享拒绝安全类型 None。这里只提供 VNC 认证（类型 2），密码 yiyiya。
 */

#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

#define VNC_SKIP_FORK 1

#define VNC_PORT 5900
#define FB_W 640
#define FB_H 480

/* 0x00RRGGBB。0xFFFFFFFF 表示字模背景透明。 */
#define COL_DESK 0x002B4C7Eu
#define COL_BAR 0x001A2332u
#define COL_DOCK 0x0014283Eu
#define COL_WIN 0x00E8EEF4u
#define COL_TITLE 0x003D5A80u
#define COL_INK 0x001A2332u
#define COL_PAPER 0x00F4F7FBu
#define COL_ACCENT 0x00E07A3Du

static uint32_t* fb;
static int mouse_x = 24;
static int mouse_y = 48;
static int mouse_btn;
static unsigned key_sym;
static int key_down;
static unsigned frame_n;
static int box_x = 500;
static int box_y = 90;
static int box_dx = 14;
static int box_dy = 10;

static int pf_bpp = 32;
static int pf_be;
static int pf_rmax = 255;
static int pf_gmax = 255;
static int pf_bmax = 255;
static int pf_rsh = 16;
static int pf_gsh = 8;
static int pf_bsh;

/* 8x8，高位在左。下标是 ASCII - 32，覆盖 32..126。 */
static const unsigned char font[95][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /*   */
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00}, /* ! */
    {0x66, 0x66, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00}, /* " */
    {0x24, 0x24, 0x7E, 0x24, 0x7E, 0x24, 0x24, 0x00}, /* # */
    {0x18, 0x3E, 0x40, 0x3C, 0x02, 0x7C, 0x18, 0x00}, /* $ */
    {0x62, 0x64, 0x08, 0x10, 0x20, 0x4C, 0x8C, 0x00}, /* % */
    {0x30, 0x48, 0x30, 0x56, 0x88, 0x88, 0x76, 0x00}, /* & */
    {0x18, 0x18, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ' */
    {0x0C, 0x10, 0x20, 0x20, 0x20, 0x10, 0x0C, 0x00}, /* ( */
    {0x30, 0x08, 0x04, 0x04, 0x04, 0x08, 0x30, 0x00}, /* ) */
    {0x00, 0x24, 0x18, 0x7E, 0x18, 0x24, 0x00, 0x00}, /* * */
    {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00}, /* + */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x10}, /* , */
    {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00}, /* - */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, /* . */
    {0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00, 0x00}, /* / */
    {0x3C, 0x42, 0x46, 0x4A, 0x52, 0x62, 0x42, 0x3C}, /* 0 */
    {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C}, /* 1 */
    {0x3C, 0x42, 0x02, 0x04, 0x18, 0x20, 0x40, 0x7E}, /* 2 */
    {0x3C, 0x42, 0x02, 0x1C, 0x02, 0x02, 0x42, 0x3C}, /* 3 */
    {0x04, 0x0C, 0x14, 0x24, 0x44, 0x7E, 0x04, 0x04}, /* 4 */
    {0x7E, 0x40, 0x40, 0x7C, 0x02, 0x02, 0x42, 0x3C}, /* 5 */
    {0x1C, 0x20, 0x40, 0x7C, 0x42, 0x42, 0x42, 0x3C}, /* 6 */
    {0x7E, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10, 0x10}, /* 7 */
    {0x3C, 0x42, 0x42, 0x3C, 0x42, 0x42, 0x42, 0x3C}, /* 8 */
    {0x3C, 0x42, 0x42, 0x42, 0x3E, 0x02, 0x04, 0x38}, /* 9 */
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00}, /* : */
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x10, 0x00}, /* ; */
    {0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x00}, /* < */
    {0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00}, /* = */
    {0x20, 0x10, 0x08, 0x04, 0x08, 0x10, 0x20, 0x00}, /* > */
    {0x3C, 0x42, 0x02, 0x0C, 0x10, 0x00, 0x10, 0x00}, /* ? */
    {0x3C, 0x42, 0x5A, 0x5A, 0x5C, 0x40, 0x3C, 0x00}, /* @ */
    {0x18, 0x24, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x00}, /* A */
    {0x7C, 0x42, 0x42, 0x7C, 0x42, 0x42, 0x42, 0x7C}, /* B */
    {0x3C, 0x42, 0x40, 0x40, 0x40, 0x40, 0x42, 0x3C}, /* C */
    {0x78, 0x44, 0x42, 0x42, 0x42, 0x42, 0x44, 0x78}, /* D */
    {0x7E, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x40, 0x7E}, /* E */
    {0x7E, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x40, 0x40}, /* F */
    {0x3C, 0x42, 0x40, 0x40, 0x4E, 0x42, 0x42, 0x3C}, /* G */
    {0x42, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x42}, /* H */
    {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C}, /* I */
    {0x1E, 0x04, 0x04, 0x04, 0x04, 0x44, 0x44, 0x38}, /* J */
    {0x42, 0x44, 0x48, 0x70, 0x48, 0x44, 0x42, 0x42}, /* K */
    {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7E}, /* L */
    {0x42, 0x66, 0x5A, 0x5A, 0x42, 0x42, 0x42, 0x42}, /* M */
    {0x42, 0x62, 0x52, 0x4A, 0x46, 0x42, 0x42, 0x42}, /* N */
    {0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C}, /* O */
    {0x7C, 0x42, 0x42, 0x7C, 0x40, 0x40, 0x40, 0x40}, /* P */
    {0x3C, 0x42, 0x42, 0x42, 0x4A, 0x44, 0x42, 0x3A}, /* Q */
    {0x7C, 0x42, 0x42, 0x7C, 0x48, 0x44, 0x42, 0x42}, /* R */
    {0x3C, 0x42, 0x40, 0x3C, 0x02, 0x02, 0x42, 0x3C}, /* S */
    {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18}, /* T */
    {0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C}, /* U */
    {0x42, 0x42, 0x42, 0x42, 0x42, 0x24, 0x18, 0x18}, /* V */
    {0x42, 0x42, 0x42, 0x42, 0x5A, 0x5A, 0x66, 0x42}, /* W */
    {0x42, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x42}, /* X */
    {0x42, 0x42, 0x24, 0x18, 0x18, 0x18, 0x18, 0x18}, /* Y */
    {0x7E, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x7E}, /* Z */
    {0x3C, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3C}, /* [ */
    {0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00, 0x00}, /* \ */
    {0x3C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x3C}, /* ] */
    {0x18, 0x24, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ^ */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E}, /* _ */
    {0x30, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ` */
    {0x00, 0x00, 0x3C, 0x02, 0x3E, 0x42, 0x46, 0x3A}, /* a */
    {0x40, 0x40, 0x5C, 0x62, 0x42, 0x42, 0x62, 0x5C}, /* b */
    {0x00, 0x00, 0x3C, 0x42, 0x40, 0x40, 0x42, 0x3C}, /* c */
    {0x02, 0x02, 0x3A, 0x46, 0x42, 0x42, 0x46, 0x3A}, /* d */
    {0x00, 0x00, 0x3C, 0x42, 0x7E, 0x40, 0x42, 0x3C}, /* e */
    {0x0C, 0x12, 0x10, 0x7C, 0x10, 0x10, 0x10, 0x10}, /* f */
    {0x00, 0x00, 0x3A, 0x46, 0x42, 0x3E, 0x02, 0x3C}, /* g */
    {0x40, 0x40, 0x5C, 0x62, 0x42, 0x42, 0x42, 0x42}, /* h */
    {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x3C}, /* i */
    {0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x44, 0x38}, /* j */
    {0x40, 0x40, 0x44, 0x48, 0x70, 0x48, 0x44, 0x42}, /* k */
    {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C}, /* l */
    {0x00, 0x00, 0x66, 0x5A, 0x5A, 0x5A, 0x42, 0x42}, /* m */
    {0x00, 0x00, 0x5C, 0x62, 0x42, 0x42, 0x42, 0x42}, /* n */
    {0x00, 0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x3C}, /* o */
    {0x00, 0x00, 0x5C, 0x62, 0x42, 0x5C, 0x40, 0x40}, /* p */
    {0x00, 0x00, 0x3A, 0x46, 0x42, 0x3A, 0x02, 0x02}, /* q */
    {0x00, 0x00, 0x5C, 0x62, 0x40, 0x40, 0x40, 0x40}, /* r */
    {0x00, 0x00, 0x3E, 0x40, 0x3C, 0x02, 0x02, 0x7C}, /* s */
    {0x10, 0x10, 0x7C, 0x10, 0x10, 0x10, 0x12, 0x0C}, /* t */
    {0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x46, 0x3A}, /* u */
    {0x00, 0x00, 0x42, 0x42, 0x42, 0x24, 0x24, 0x18}, /* v */
    {0x00, 0x00, 0x42, 0x42, 0x5A, 0x5A, 0x66, 0x42}, /* w */
    {0x00, 0x00, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42}, /* x */
    {0x00, 0x00, 0x42, 0x42, 0x46, 0x3A, 0x02, 0x3C}, /* y */
    {0x00, 0x00, 0x7E, 0x04, 0x08, 0x10, 0x20, 0x7E}, /* z */
    {0x0C, 0x10, 0x10, 0x60, 0x10, 0x10, 0x10, 0x0C}, /* { */
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18}, /* | */
    {0x30, 0x08, 0x08, 0x06, 0x08, 0x08, 0x08, 0x30}, /* } */
    {0x32, 0x4C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ~ */
};

static void step(const char* s) {
  int n = 0;
  while (s[n] != 0) n++;
  if (n > 0) write(2, s, (size_t)n);
}

static int wr_full(int fd, const void* buf, int n) {
  const unsigned char* p = buf;
  int off = 0;
  unsigned eagain = 0;
  while (off < n) {
    int w = write(fd, p + off, (size_t)(n - off));
    if (w < 0) {
      /* 【EAGAIN 让出 CPU + 防死连接】发送缓冲满时 return EAGAIN；睡一小拍
       * 让出（否则占满单核饿死 shell），等 ACK 腾空间再重试。若同一位置连续
       * EAGAIN 太多（对端已死/不发 ACK），说明这条连接没救了，直接返回 -1，
       * 让上层 close 掉、accept 新连接，避免无限重试把整机拖死。 */
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        eagain++;
        if (eagain > 4000) {
          return -1;
        }
        usleep(200);
        continue;
      }
      return -1;
    }
    if (w == 0) return -1;
    eagain = 0;
    off += w;
  }
  return 0;
}

static int rd_full(int fd, void* buf, int n) {
  unsigned char* p = buf;
  int off = 0;
  while (off < n) {
    int r = read(fd, p + off, (size_t)(n - off));
    if (r < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        usleep(1000);   /* 同上：EAGAIN 让出 CPU，别饿死同核线程 */
        continue;
      }
      return -1;
    }
    if (r == 0) return -1;
    off += r;
  }
  return 0;
}

static void be16(unsigned char* d, unsigned v) {
  d[0] = (unsigned char)(v >> 8);
  d[1] = (unsigned char)v;
}

static void be32(unsigned char* d, unsigned v) {
  d[0] = (unsigned char)(v >> 24);
  d[1] = (unsigned char)(v >> 16);
  d[2] = (unsigned char)(v >> 8);
  d[3] = (unsigned char)v;
}

static unsigned u16be(const unsigned char* d) {
  return ((unsigned)d[0] << 8) | d[1];
}

static unsigned u32be(const unsigned char* d) {
  return ((unsigned)d[0] << 24) | ((unsigned)d[1] << 16) |
         ((unsigned)d[2] << 8) | d[3];
}

/* VNC 认证：密码每个字节先按位颠倒，再当 DES 密钥加密 16 字节挑战。 */
#define VNC_PASS "yiyiya"

static const unsigned char des_s[8][64] = {
    {14, 4,  13, 1, 2,  15, 11, 8,  3,  10, 6,  12, 5,  9,  0, 7,  0,  15, 7,
     4,  14, 2,  13, 1, 10, 6,  12, 11, 9,  5,  3,  8,  4,  1,  14, 8,  13, 6,
     2,  11, 15, 12, 9, 7,  3,  10, 5,  0,  15, 12, 8,  2,  4,  9,  1,  7,  5,
     11, 3,  14, 10, 0, 6,  13},
    {15, 1, 8,  14, 6,  11, 3, 4,  9,  7, 2,  13, 12, 0, 5,  10, 3,  13, 4, 7, 15,
     2,  8, 14, 12, 0,  1,  10, 6,  9, 11, 5,  0,  14, 7,  11, 10, 4,  13, 1, 5,
     8,  12, 6, 9,  3,  2,  15, 13, 8, 10, 1,  3,  15, 4,  2,  11, 6,  7,  12, 0,
     5,  14, 9},
    {10, 0, 9, 14, 6, 3,  15, 5,  1,  13, 12, 7, 11, 4,  2,  8,  13, 7,  0,  9, 3,
     4,  6, 10, 2, 8, 5,  14, 12, 11, 15, 1,  13, 6,  4,  9,  8,  15, 3, 0, 11, 1,
     2,  12, 5, 10, 14, 7, 1,  10, 13, 0,  6,  9,  8,  7,  4,  15, 14, 3, 11, 5,
     2,  12},
    {7, 13, 14, 3, 0, 6,  9, 10, 1, 2,  8,  5, 11, 12, 4,  15, 13, 8, 11, 5,  6,
     15, 0, 3,  4, 7, 2,  12, 1, 10, 14, 9,  10, 6,  9,  0,  12, 11, 7,  13, 15,
     1,  3, 14, 5, 2, 8,  4,  3,  15, 0,  6,  10, 1,  13, 8,  9,  4,  5,  11, 12,
     7,  2, 14},
    {2,  12, 4,  1,  7,  10, 11, 6,  8, 5,  3,  15, 13, 0, 14, 9,  14, 11, 2,  12,
     4,  7,  13, 1,  5,  0,  15, 10, 3, 9,  8,  6,  4,  2,  1,  11, 10, 13, 7,
     8,  15, 9,  12, 5,  6,  3,  0,  14, 11, 8, 12, 7,  1,  14, 2,  13, 6,  15,
     0,  9,  10, 4,  5,  3},
    {12, 1,  10, 15, 9, 2,  6,  8,  0, 13, 3, 4,  14, 7,  5,  11, 10, 15, 4,  2,  7,
     12, 9,  5,  6,  1,  13, 14, 0, 11, 3, 8,  9,  14, 15, 5,  2,  8,  12, 3,  7,
     0,  4,  10, 1,  13, 11, 6,  4, 3,  2,  12, 9,  5,  15, 10, 11, 14, 1,  7,
     6,  0,  8,  13},
    {4, 11, 2,  14, 15, 0, 8,  13, 3, 12, 9,  7, 5,  10, 6,  1,  13, 0,  11, 7,  4,
     9, 1,  10, 14, 3,  5,  12, 2, 15, 8, 6,  1,  4,  11, 13, 12, 3,  7,  14, 10,
     15, 6, 8,  0,  5,  9,  2,  6, 11, 13, 8, 1,  4,  10, 7,  9,  5,  0,  15, 14,
     2,  3, 12},
    {13, 2, 8, 4,  6,  15, 11, 1,  10, 9, 3, 14, 5,  0, 12, 7, 1,  15, 13, 8, 10,
     3,  7, 4, 12, 5,  6,  11, 0,  14, 9, 2,  7, 11, 4,  1, 9,  12, 14, 2,  0, 6,
     10, 13, 15, 3, 5,  8,  2,  1,  14, 7, 4, 10, 8,  13, 15, 12, 9,  0,  3, 5,
     6,  11},
};
static const unsigned char des_ip[64] = {
    58, 50, 42, 34, 26, 18, 10, 2,  60, 52, 44, 36, 28, 20, 12, 4,
    62, 54, 46, 38, 30, 22, 14, 6,  64, 56, 48, 40, 32, 24, 16, 8,
    57, 49, 41, 33, 25, 17, 9,  1,  59, 51, 43, 35, 27, 19, 11, 3,
    61, 53, 45, 37, 29, 21, 13, 5,  63, 55, 47, 39, 31, 23, 15, 7};
static const unsigned char des_fp[64] = {
    40, 8, 48, 16, 56, 24, 64, 32, 39, 7, 47, 15, 55, 23, 63, 31,
    38, 6, 46, 14, 54, 22, 62, 30, 37, 5, 45, 13, 53, 21, 61, 29,
    36, 4, 44, 12, 52, 20, 60, 28, 35, 3, 43, 11, 51, 19, 59, 27,
    34, 2, 42, 10, 50, 18, 58, 26, 33, 1, 41, 9,  49, 17, 57, 25};
static const unsigned char des_e[48] = {
    32, 1,  2,  3,  4,  5,  4,  5,  6,  7,  8,  9,  8,  9,  10, 11,
    12, 13, 12, 13, 14, 15, 16, 17, 16, 17, 18, 19, 20, 21, 20, 21,
    22, 23, 24, 25, 24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32, 1};
static const unsigned char des_p[32] = {
    16, 7, 20, 21, 29, 12, 28, 17, 1,  15, 23, 26, 5,  18, 31, 10,
    2,  8, 24, 14, 32, 27, 3,  9,  19, 13, 30, 6,  22, 11, 4,  25};
static const unsigned char des_pc1[56] = {
    57, 49, 41, 33, 25, 17, 9,  1,  58, 50, 42, 34, 26, 18, 10, 2,  59, 51, 43,
    35, 27, 19, 11, 3,  60, 52, 44, 36, 63, 55, 47, 39, 31, 23, 15, 7,  62, 54,
    46, 38, 30, 22, 14, 6,  61, 53, 45, 37, 29, 21, 13, 5,  28, 20, 12, 4};
static const unsigned char des_pc2[48] = {
    14, 17, 11, 24, 1,  5,  3,  28, 15, 6,  21, 10, 23, 19, 12, 4,
    26, 8,  16, 7,  27, 20, 13, 2,  41, 52, 31, 37, 47, 55, 30, 40,
    51, 45, 33, 48, 44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32};
static const unsigned char des_sh[16] = {1, 1, 2, 2, 2, 2, 2, 2,
                                         1, 2, 2, 2, 2, 2, 2, 1};

static void des_perm(const unsigned char* in, const unsigned char* table, int n,
                     unsigned char* out) {
  int i;
  for (i = 0; i < n; i++) out[i] = in[table[i] - 1];
}

static void des_encrypt_block(const unsigned char key[8], unsigned char block[8]) {
  unsigned char kb[64], k56[56], bits[64], ipb[64], l[32], r[32], er[48];
  unsigned char sub[16][48], fout[32], nr[32], pre[64];
  unsigned char c[28], d[28];
  int i, b, round, s;

  for (i = 0; i < 8; i++) {
    for (b = 0; b < 8; b++) kb[i * 8 + b] = (key[i] >> (7 - b)) & 1;
  }
  des_perm(kb, des_pc1, 56, k56);
  for (i = 0; i < 28; i++) {
    c[i] = k56[i];
    d[i] = k56[28 + i];
  }
  for (round = 0; round < 16; round++) {
    int sh = des_sh[round];
    unsigned char cs[28], ds[28], cd[56];
    for (i = 0; i < 28; i++) {
      cs[i] = c[(i + sh) % 28];
      ds[i] = d[(i + sh) % 28];
    }
    for (i = 0; i < 28; i++) {
      c[i] = cs[i];
      d[i] = ds[i];
      cd[i] = c[i];
      cd[28 + i] = d[i];
    }
    des_perm(cd, des_pc2, 48, sub[round]);
  }
  for (i = 0; i < 8; i++) {
    for (b = 0; b < 8; b++) bits[i * 8 + b] = (block[i] >> (7 - b)) & 1;
  }
  des_perm(bits, des_ip, 64, ipb);
  for (i = 0; i < 32; i++) {
    l[i] = ipb[i];
    r[i] = ipb[32 + i];
  }
  for (round = 0; round < 16; round++) {
    des_perm(r, des_e, 48, er);
    for (i = 0; i < 48; i++) er[i] ^= sub[round][i];
    for (s = 0; s < 8; s++) {
      int row = (er[s * 6] << 1) | er[s * 6 + 5];
      int col = (er[s * 6 + 1] << 3) | (er[s * 6 + 2] << 2) |
                (er[s * 6 + 3] << 1) | er[s * 6 + 4];
      int val = des_s[s][row * 16 + col];
      for (b = 0; b < 4; b++) fout[s * 4 + b] = (val >> (3 - b)) & 1;
    }
    des_perm(fout, des_p, 32, nr);
    for (i = 0; i < 32; i++) nr[i] ^= l[i];
    for (i = 0; i < 32; i++) {
      l[i] = r[i];
      r[i] = nr[i];
    }
  }
  for (i = 0; i < 32; i++) {
    pre[i] = r[i];
    pre[32 + i] = l[i];
  }
  des_perm(pre, des_fp, 64, bits);
  for (i = 0; i < 8; i++) {
    unsigned v = 0;
    for (b = 0; b < 8; b++) v = (v << 1) | bits[i * 8 + b];
    block[i] = (unsigned char)v;
  }
}

static int des_ok(void) {
  unsigned char key[8] = {0x13, 0x34, 0x57, 0x79, 0x9b, 0xbc, 0xdf, 0xf1};
  unsigned char blk[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  static const unsigned char expect[8] = {0x85, 0xe8, 0x13, 0x54,
                                          0x0f, 0x0a, 0xb4, 0x05};
  des_encrypt_block(key, blk);
  return memcmp(blk, expect, 8) == 0;
}

static void vnc_des_key(unsigned char key[8]) {
  int i, b;
  for (i = 0; i < 8; i++) {
    unsigned char c = (i < 6) ? (unsigned char)VNC_PASS[i] : 0;
    unsigned char r = 0;
    for (b = 0; b < 8; b++) {
      if (c & (1u << b)) r |= (unsigned char)(1u << (7 - b));
    }
    key[i] = r;
  }
}

static int security_result(int fd, int ok, int v38, const char* why) {
  unsigned char b[4];
  be32(b, ok ? 0 : 1);
  if (wr_full(fd, b, 4) < 0) return -1;
  if (!ok && v38 && why != NULL) {
    int n = 0;
    while (why[n] != 0) n++;
    be32(b, (unsigned)n);
    if (wr_full(fd, b, 4) < 0) return -1;
    if (n > 0 && wr_full(fd, why, n) < 0) return -1;
  }
  return ok ? 0 : -1;
}

static int vnc_auth(int fd, int v38) {
  unsigned char challenge[16];
  unsigned char expect[16];
  unsigned char got[16];
  unsigned char key[8];
  unsigned seed = 0x6d2b79f5u;
  int i;
  if (!des_ok()) {
    step("test-vnc: des bad\n");
    return security_result(fd, 0, v38, "des");
  }
  for (i = 0; i < 16; i++) {
    seed = seed * 1664525u + 1013904223u;
    challenge[i] = (unsigned char)(seed >> 16);
  }
  vnc_des_key(key);
  memcpy(expect, challenge, 16);
  des_encrypt_block(key, expect);
  des_encrypt_block(key, expect + 8);
  step("test-vnc: auth challenge\n");
  if (wr_full(fd, challenge, 16) < 0) return -1;
  if (rd_full(fd, got, 16) < 0) return -1;
  if (memcmp(got, expect, 16) != 0) {
    step("test-vnc: auth failed\n");
    return security_result(fd, 0, v38, "password");
  }
  step("test-vnc: auth ok\n");
  return security_result(fd, 1, v38, NULL);
}

static void fill(int x, int y, int w, int h, uint32_t c) {
  int x0, y0, x1, y1, yy, xx;
  if (fb == NULL || w <= 0 || h <= 0) return;
  x0 = x < 0 ? 0 : x;
  y0 = y < 0 ? 0 : y;
  x1 = x + w;
  y1 = y + h;
  if (x1 > FB_W) x1 = FB_W;
  if (y1 > FB_H) y1 = FB_H;
  for (yy = y0; yy < y1; yy++) {
    uint32_t* row = fb + yy * FB_W;
    for (xx = x0; xx < x1; xx++) row[xx] = c;
  }
}

static void glyph(int x, int y, int ch, uint32_t fg, uint32_t bg, int scale) {
  const unsigned char* g;
  int row, col, sy, sx;
  if (ch < 32 || ch > 126) ch = '?';
  g = font[ch - 32];
  for (row = 0; row < 8; row++) {
    unsigned char bits = g[row];
    for (col = 0; col < 8; col++) {
      uint32_t c = (bits & (unsigned char)(0x80 >> col)) ? fg : bg;
      if (c == 0xFFFFFFFFu) continue;
      for (sy = 0; sy < scale; sy++) {
        for (sx = 0; sx < scale; sx++) {
          int px = x + col * scale + sx;
          int py = y + row * scale + sy;
          if ((unsigned)px < FB_W && (unsigned)py < FB_H)
            fb[py * FB_W + px] = c;
        }
      }
    }
  }
}

static void text(int x, int y, const char* s, uint32_t fg, uint32_t bg,
                 int scale) {
  int i;
  for (i = 0; s[i] != 0; i++) {
    glyph(x, y, (unsigned char)s[i], fg, bg, scale);
    x += 8 * scale;
  }
}

static void draw_desktop(void) {
  char line[64];
  int i;
  static const uint32_t icons[] = {0x00E07A3Du, 0x003DDC97u, 0x004C8BF5u,
                                   0x00F2C14Eu};

  frame_n++;
  box_x += box_dx;
  box_y += box_dy;
  if (box_x < 470 || box_x > FB_W - 36) box_dx = -box_dx;
  if (box_y < 52 || box_y > FB_H - 36) box_dy = -box_dy;

  fill(0, 0, FB_W, FB_H, COL_DESK);
  fill(0, 0, FB_W, 40, COL_BAR);
  fill(0, 40, 64, FB_H - 40, COL_DOCK);
  text(16, 8, "YiYiYa", COL_PAPER, COL_BAR, 2);
  text(200, 12, "vnc://192.168.1.250:5900", COL_PAPER, COL_BAR, 1);

  for (i = 0; i < 4; i++) {
    fill(12, 56 + i * 52, 40, 40, icons[i]);
  }

  fill(88, 64, 360, 220, COL_WIN);
  fill(88, 64, 360, 32, COL_TITLE);
  fill(420, 72, 16, 16, COL_ACCENT);
  text(100, 70, "desktop", COL_PAPER, COL_TITLE, 2);

  snprintf(line, sizeof(line), "frame %u", frame_n);
  text(104, 112, line, COL_INK, COL_WIN, 2);
  snprintf(line, sizeof(line), "xy %d,%d %c%c%c", mouse_x, mouse_y,
           (mouse_btn & 1) ? 'L' : '-', (mouse_btn & 2) ? 'M' : '-',
           (mouse_btn & 4) ? 'R' : '-');
  text(104, 148, line, COL_INK, COL_WIN, 2);
  if (key_sym >= 32 && key_sym < 127) {
    snprintf(line, sizeof(line), "key %c %s", (char)key_sym,
             key_down ? "down" : "up");
  } else {
    snprintf(line, sizeof(line), "key %u %s", key_sym, key_down ? "down" : "up");
  }
  text(104, 184, line, COL_INK, COL_WIN, 2);
  text(104, 228, "move the pointer", COL_INK, COL_WIN, 1);

  fill(box_x, box_y, 28, 28, COL_ACCENT);
}

static void pack_pixel(unsigned char* d, unsigned r, unsigned g, unsigned b) {
  unsigned rv, gv, bv, pix;
  int n, i;
  rv = pf_rmax == 255 ? r : r * (unsigned)pf_rmax / 255;
  gv = pf_gmax == 255 ? g : g * (unsigned)pf_gmax / 255;
  bv = pf_bmax == 255 ? b : b * (unsigned)pf_bmax / 255;
  pix = (rv << pf_rsh) | (gv << pf_gsh) | (bv << pf_bsh);
  n = pf_bpp / 8;
  if (n != 1 && n != 2 && n != 4) n = 4;
  if (pf_be) {
    for (i = 0; i < n; i++) d[i] = (unsigned char)(pix >> (8 * (n - 1 - i)));
  } else {
    for (i = 0; i < n; i++) d[i] = (unsigned char)(pix >> (8 * i));
  }
}

static int painted;
static int prev_box_x, prev_box_y, prev_mx, prev_my;

static uint32_t pixel_at(int x, int y) {
  uint32_t c = fb[y * FB_W + x];
  if (x >= mouse_x && x < mouse_x + 10 && y >= mouse_y && y < mouse_y + 10)
    c = 0x00FFFFFFu;
  return c;
}

static int send_raw_rect(int fd, int rx, int ry, int rw, int rh) {
  unsigned char hdr[12];
  unsigned char chunk[4096];
  unsigned char px[4];
  int bpp, x, y, filled;

  if (rx < 0) {
    rw += rx;
    rx = 0;
  }
  if (ry < 0) {
    rh += ry;
    ry = 0;
  }
  if (rx >= FB_W || ry >= FB_H || rw < 1 || rh < 1) {
    rx = 0;
    ry = 0;
    rw = 1;
    rh = 1;
  }
  if (rx + rw > FB_W) rw = FB_W - rx;
  if (ry + rh > FB_H) rh = FB_H - ry;
  if (rw < 1) rw = 1;
  if (rh < 1) rh = 1;

  /* 【x/y 必须偶对齐】RFB 规范要求 Rectangle 的 x-position/y-position 为偶数；
   * 鼠标坐标来自客户端（PointerEvent）可能为奇数，直接发会让客户端拒绝/错乱。
   * 向下对齐到偶数，并把宽度/高度补足，保证覆盖原区域。 */
  if (rx & 1) { rx -= 1; rw += 1; }
  if (ry & 1) { ry -= 1; rh += 1; }
  if (rx < 0) { rx = 0; }
  if (ry < 0) { ry = 0; }
  if (rx + rw > FB_W) rw = FB_W - rx;
  if (ry + rh > FB_H) rh = FB_H - ry;

  bpp = pf_bpp / 8;
  if (bpp != 1 && bpp != 2 && bpp != 4) bpp = 4;
  be16(hdr + 0, (unsigned)rx);
  be16(hdr + 2, (unsigned)ry);
  be16(hdr + 4, (unsigned)rw);
  be16(hdr + 6, (unsigned)rh);
  be32(hdr + 8, 0);
  if (wr_full(fd, hdr, 12) < 0) return -1;

  filled = 0;
  int _ck = 0;
  for (y = ry; y < ry + rh; y++) {
    for (x = rx; x < rx + rw; x++) {
      uint32_t c = pixel_at(x, y);
      if (filled + bpp > (int)sizeof(chunk)) {
        if (_ck == 0 || (_ck % 8) == 0) {
          char b[48];
          sprintf(b, "test-vnc: chunk %d\n", _ck);
          step(b);
        }
        _ck++;
        if (wr_full(fd, chunk, filled) < 0) return -1;
        filled = 0;
      }
      pack_pixel(px, (c >> 16) & 255, (c >> 8) & 255, c & 255);
      memcpy(chunk + filled, px, (size_t)bpp);
      filled += bpp;
    }
  }
  if (filled > 0 && wr_full(fd, chunk, filled) < 0) return -1;
  {
    char b[48];
    sprintf(b, "test-vnc: chunks total %d\n", _ck);
    step(b);
  }
  return 0;
}

/* 第一帧整屏。之后只送文字、移动方块和光标，避免每帧重传 1.2MB。 */
static int send_frame(int fd, int incremental) {
  unsigned char hdr[4];

  draw_desktop();
  if (frame_n == 1) step("test-vnc: frame\n");
  printf("test-vnc: send_frame painted=%d inc=%d box=%d,%d prev=%d,%d\n",
         painted, incremental, box_x, box_y, prev_box_x, prev_box_y);
  hdr[0] = 0;
  hdr[1] = 0;
  if (!painted || !incremental) {
    be16(hdr + 2, 1);
    step("test-vnc: full hdr...\n");
    if (wr_full(fd, hdr, 4) < 0) return -1;
    step("test-vnc: full body start\n");
    if (send_raw_rect(fd, 0, 0, FB_W, FB_H) < 0) return -1;
    step("test-vnc: full body done\n");
    painted = 1;
  } else {
    /* 【nrects 必须与实际发送的矩形数一致】下面只发 4 个矩形（方块前后位
     * + 光标前后位）。原来这里写 5 是因为还包含 100,108,340x100 的固定文字
     * 区；删掉那块后必须同步改成 4，否则客户端等第 5 个矩形却收不到，
     * 协议错乱直接 RST（实测 errno=104 ECONNRESET）。 */
    be16(hdr + 2, 4);
    if (wr_full(fd, hdr, 4) < 0) return -1;
    if (send_raw_rect(fd, prev_box_x, prev_box_y, 28, 28) < 0) return -1;
    if (send_raw_rect(fd, box_x, box_y, 28, 28) < 0) return -1;
    if (send_raw_rect(fd, prev_mx, prev_my, 10, 10) < 0) return -1;
    if (send_raw_rect(fd, mouse_x, mouse_y, 10, 10) < 0) return -1;
  }
  prev_box_x = box_x;
  prev_box_y = box_y;
  prev_mx = mouse_x;
  prev_my = mouse_y;
  printf("test-vnc: send_frame done\n");
  return 0;
}

static void set_format(const unsigned char* f) {
  int bpp = f[0];
  int rmax, gmax, bmax;
  if (bpp != 8 && bpp != 16 && bpp != 32) return;
  rmax = (int)u16be(f + 4);
  gmax = (int)u16be(f + 6);
  bmax = (int)u16be(f + 8);
  if (rmax <= 0 || gmax <= 0 || bmax <= 0) return;
  pf_bpp = bpp;
  pf_be = f[2] ? 1 : 0;
  pf_rmax = rmax;
  pf_gmax = gmax;
  pf_bmax = bmax;
  pf_rsh = f[10] & 31;
  pf_gsh = f[11] & 31;
  pf_bsh = f[12] & 31;
}

static int handshake(int fd) {
  unsigned char ver[12];
  unsigned char buf[32];
  unsigned char sec;
  int minor, v38;
  const char* name = "YiYiYa";

  step("test-vnc: banner\n");
  if (wr_full(fd, "RFB 003.008\n", 12) < 0) {
    step("test-vnc: banner write failed\n");
    return -1;
  }
  step("test-vnc: banner sent\n");
  if (rd_full(fd, ver, 12) < 0) return -1;
  printf("test-vnc: ver %c%c%c.%c%c%c\n", ver[4], ver[5], ver[6], ver[8], ver[9],
         ver[10]);
  if (memcmp(ver, "RFB 003.", 8) != 0) return -1;
  minor = (ver[8] - '0') * 100 + (ver[9] - '0') * 10 + (ver[10] - '0');
  /* 003.889 是 macOS 屏幕共享的版本号，握手按 3.8。 */
  v38 = (minor >= 7);
  /* 只提供类型 2。类型 1（None）会被 macOS 屏幕共享直接拒绝。 */
  if (v38) {
    buf[0] = 1;
    buf[1] = 2;
    if (wr_full(fd, buf, 2) < 0) return -1;
    if (rd_full(fd, &sec, 1) < 0) return -1;
    printf("test-vnc: security %u\n", sec);
    if (sec != 2) {
      security_result(fd, 0, 1, "need VNC auth");
      return -1;
    }
  } else {
    be32(buf, 2);
    if (wr_full(fd, buf, 4) < 0) return -1;
  }
  if (vnc_auth(fd, v38) < 0) return -1;
  if (rd_full(fd, buf, 1) < 0) return -1;

  be16(buf, FB_W);
  be16(buf + 2, FB_H);
  buf[4] = 32;
  buf[5] = 24;
  buf[6] = 0;
  buf[7] = 1;
  be16(buf + 8, 255);
  be16(buf + 10, 255);
  be16(buf + 12, 255);
  buf[14] = 16;
  buf[15] = 8;
  buf[16] = 0;
  buf[17] = 0;
  buf[18] = 0;
  buf[19] = 0;
  be32(buf + 20, 6);
  memcpy(buf + 24, name, 6);
  return wr_full(fd, buf, 30);
}

static int skip_n(int fd, int n) {
  unsigned char tmp[64];
  while (n > 0) {
    int k = n > (int)sizeof(tmp) ? (int)sizeof(tmp) : n;
    if (rd_full(fd, tmp, k) < 0) return -1;
    n -= k;
  }
  return 0;
}

static int client_loop(int fd) {
  for (;;) {
    unsigned char type;
    unsigned char msg[20];
    unsigned count, n;
    static unsigned lc = 0;
    if (rd_full(fd, &type, 1) < 0) {
      printf("test-vnc: loop rd fail lc=%u\n", lc);
      return -1;
    }
    lc++;
    if (lc <= 40 && (type != 3 || (lc % 10) == 0)) {
      printf("test-vnc: loop #%u type=%u\n", lc, type);
    }
    if (type == 0) {
      if (rd_full(fd, msg, 19) < 0) return -1;
      set_format(msg + 3);
    } else if (type == 2) {
      if (rd_full(fd, msg, 3) < 0) return -1;
      count = u16be(msg + 1);
      if (skip_n(fd, (int)count * 4) < 0) return -1;
    } else if (type == 3) {
      if (rd_full(fd, msg, 9) < 0) return -1;
      if (send_frame(fd, msg[0]) < 0) {
        printf("test-vnc: loop send_frame fail lc=%u\n", lc);
        return -1;
      }
    } else if (type == 4) {
      if (rd_full(fd, msg, 7) < 0) return -1;
      key_down = msg[0] ? 1 : 0;
      key_sym = u32be(msg + 3);
    } else if (type == 5) {
      if (rd_full(fd, msg, 5) < 0) return -1;
      mouse_btn = msg[0];
      mouse_x = (int)u16be(msg + 1);
      mouse_y = (int)u16be(msg + 3);
      if (mouse_x < 0) mouse_x = 0;
      if (mouse_y < 0) mouse_y = 0;
      if (mouse_x >= FB_W) mouse_x = FB_W - 1;
      if (mouse_y >= FB_H) mouse_y = FB_H - 1;
    } else if (type == 6) {
      if (rd_full(fd, msg, 7) < 0) return -1;
      n = u32be(msg + 3);
      if (skip_n(fd, (int)n) < 0) return -1;
    } else if (type == 150) {
      if (rd_full(fd, msg, 8) < 0) return -1;
    } else {
      printf("test-vnc: unknown msg %u\n", type);
      return -1;
    }
  }
}

/* 640*480*4 一次 mmap 一块连续缓冲。内核 sys_mmap2 的 anon 分支对整段
 * 做 valloc（逐页 page_map_on），单次系统调用即可拿到连续空间；不必
 * 再"先一页再倒推 299 次 MAP_FIXED"——那样会与已有映射冲突反复失败，
 * 且每次失败泄漏。 */
static uint32_t* map_fb(void) {
  const size_t bytes = (size_t)FB_W * FB_H * sizeof(uint32_t);
  void* p = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (p == MAP_FAILED) {
    return NULL;
  }
  return (uint32_t*)p;
}

static void handle_client(int c) {
  int one = 1;
  pf_bpp = 32;
  pf_be = 0;
  painted = 0;
  frame_n = 0;
  pf_rmax = pf_gmax = pf_bmax = 255;
  pf_rsh = 16;
  pf_gsh = 8;
  pf_bsh = 0;
  setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  step("test-vnc: handle\n");
  step("test-vnc: handshake\n");
  if (handshake(c) != 0) {
    step("test-vnc: handshake failed\n");
    step("test-vnc: client closed\n");
    return;
  }
  if (fb == NULL) {
    step("test-vnc: fb\n");
    fb = map_fb();
    if (fb == NULL) {
      step("test-vnc: no memory\n");
      return;
    }
    step("test-vnc: fb done\n");
  }
  client_loop(c);
  step("test-vnc: client closed\n");
}

static void serve(int srv) {
  for (;;) {
    struct sockaddr_in peer;
    socklen_t plen = sizeof(peer);
    int c = accept(srv, (struct sockaddr*)&peer, &plen);
    int err = errno;
    unsigned char* ip;
    /* 没人连接时 accept 返回 EAGAIN。错误码先留下，后面不再调会改 errno 的函数。 */
    if (c < 0) {
      if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK) {
        /* 【让出 CPU】accept EAGAIN 裸 continue 会占满单核把 shell/init 饿死。 */
        usleep(2000);
        continue;
      }
      errno = err;
      perror("accept");
      break;
    }
    ip = (unsigned char*)&peer.sin_addr.s_addr;
    printf("test-vnc: client %u.%u.%u.%u\n", ip[0], ip[1], ip[2], ip[3]);
    /* 就在监听进程里握手。每个连接再 fork 会复制整份页表，并且按
     * ksp_size 分配内核栈；ksp_size 被踩坏时内核直接断言停机。 */
    handle_client(c);
    close(c);
  }
}

int main(void) {
  int srv;
  int on = 1;
  struct sockaddr_in addr;

  step("test-vnc: socket\n");
  srv = socket(AF_INET, SOCK_STREAM, 0);
  if (srv < 0) {
    perror("socket");
    return 1;
  }
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(VNC_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  step("test-vnc: bind\n");
  if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(srv);
    return 1;
  }
  step("test-vnc: listen\n");
  if (listen(srv, 4) < 0) {
    perror("listen");
    close(srv);
    return 1;
  }
  step("test-vnc: vnc://192.168.1.250:5900/\n");
  step("test-vnc: password yiyiya\n");
  step("test-vnc: do not run test-net, the stack owns /dev/net\n");
#ifdef VNC_SKIP_FORK
  /* 【规避已知问题】fork 在 raspi5(aarch64) 上会破坏子进程用户栈上的 srv
   * （变成 -9），accept(srv) 随即失败刷屏。此处不 fork，直接在监听进程里
   * serve，功能可用。见 zdata/faq.md Q10：fork bug 待修。 */
  step("test-vnc: serve (skip fork)\n");
  serve(srv);
#else
  step("test-vnc: fork\n");
  {
    int pid = fork();
    if (pid < 0) {
      perror("fork");
      return 1;
    } else if (pid > 0) {
      step("test-vnc: parent exit\n");
      return 0;
    }
  }
  step("test-vnc: child accept\n");
  serve(srv);
#endif
  close(srv);
  return 1;
}
