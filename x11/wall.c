/*******************************************************************
 * wall — 桌面背景设置（自研 X11 桌面组件之一）。
 *
 * 等价于 feh --bg-fill / hsetroot，但直接用 x11.c 的协议层，不依赖
 * libX11/GTK/libfm。做法：把 BMP 画进一个和屏幕等大的 pixmap，再把它
 * 设为根窗口背景；pixmap 由根窗口持有引用，进程退出后仍在 ⇒ 背景常驻。
 *
 * 用法：wall <file.bmp> [x11-socket]
 *   BMP 需为 24/32bpp 未压缩（macOS 上可用 sips 生成：
 *   sips -s format bmp -z 768 1024 in.jpg --out out.bmp）
 ********************************************************************/
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "x11.h"

#define BAND_ROWS 8

struct bmp {
  int w, h;        /* h > 0 自下而上；h < 0 自上而下 */
  int bpp;
  uint32_t off;    /* 像素数据起点 */
  uint32_t stride; /* 行字节（4 字节对齐） */
};

static uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t le32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static int bmp_parse(const uint8_t* d, uint32_t n, struct bmp* b) {
  if (n < 54 || d[0] != 'B' || d[1] != 'M') {
    return -1;
  }
  b->off = le32(d + 10);
  b->w = (int)le32(d + 18);
  b->h = (int)le32(d + 22);
  b->bpp = (int)le16(d + 28);
  if (le32(d + 30) != 0) { /* 只吃 BI_RGB */
    return -1;
  }
  if ((b->bpp != 24 && b->bpp != 32) || b->w <= 0 || b->h == 0) {
    return -1;
  }
  b->stride = (uint32_t)(((b->w * (b->bpp / 8)) + 3) & ~3u);
  {
    uint32_t rows = (uint32_t)(b->h < 0 ? -b->h : b->h);
    if ((uint64_t)b->off + (uint64_t)b->stride * rows > n) {
      return -1;
    }
  }
  return 0;
}

int main(int argc, char* argv[]) {
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  static uint8_t filebuf[5 * 1024 * 1024];
  const char* path;
  const char* node = NULL;
  x11_t c;
  struct bmp b;
  int fd;
  long total = 0;
  uint32_t pid, gc;
  uint8_t* row;
  int bpp, img_w, img_h, topdown, src_x, src_y, copy_w, copy_h, dst_x, dst_y;
  int band, y;

  if (argc < 2) {
    fprintf(stderr, "usage: wall <file.bmp> [x11-socket]\n");
    return 2;
  }
  path = argv[1];
  if (argc > 2) {
    node = argv[2];
  }

  fd = open(path, O_RDONLY);
  if (fd < 0) {
    perror("wall: open");
    return 1;
  }
  while (total < (long)sizeof(filebuf)) {
    ssize_t n = read(fd, filebuf + total, sizeof(filebuf) - (size_t)total);
    if (n > 0) {
      total += n;
    } else if (n == 0) {
      break;
    } else {
      perror("wall: read");
      close(fd);
      return 1;
    }
  }
  close(fd);
  if (bmp_parse(filebuf, (uint32_t)total, &b) < 0) {
    fprintf(stderr, "wall: %s 不是 24/32bpp 未压缩 BMP\n", path);
    return 1;
  }

  if (x11_connect(&c, node) < 0) {
    return 1;
  }
  printf("wall: %dx%d %dbpp -> display %dx%d (depth %d)\n", b.w, b.h, b.bpp, c.w,
         c.h, (int)c.root_depth);
  if (c.w <= 0 || c.h <= 0 || c.w > 4096) {
    fprintf(stderr, "wall: 屏幕尺寸异常\n");
    return 1;
  }

  pid = x11_id(&c);
  gc = x11_id(&c);
  if (x11_create_pixmap(&c, pid, (uint8_t)c.root_depth, c.w, c.h) < 0 ||
      x11_create_gc(&c, gc, pid) < 0) {
    fprintf(stderr, "wall: CreatePixmap/CreateGC 失败\n");
    return 1;
  }

  /* 图比屏幕大就居中裁剪，比屏幕小就居中（不做缩放：省掉插值代码） */
  bpp = b.bpp / 8;
  img_w = b.w;
  img_h = (b.h < 0) ? -b.h : b.h;
  topdown = (b.h < 0);
  src_x = (img_w > c.w) ? (img_w - c.w) / 2 : 0;
  src_y = (img_h > c.h) ? (img_h - c.h) / 2 : 0;
  copy_w = (img_w > c.w) ? c.w : img_w;
  copy_h = (img_h > c.h) ? c.h : img_h;
  dst_x = (c.w - copy_w) / 2;
  dst_y = (c.h - copy_h) / 2;

  row = (uint8_t*)malloc((size_t)c.w * 4u * BAND_ROWS);
  if (row == NULL) {
    fprintf(stderr, "wall: 内存不足\n");
    return 1;
  }
  memset(row, 0, (size_t)c.w * 4u * BAND_ROWS);
  /* 先整屏涂黑：pixmap 未初始化的内容是垃圾 */
  for (y = 0; y < c.h; y += BAND_ROWS) {
    int rows = (c.h - y < BAND_ROWS) ? (c.h - y) : BAND_ROWS;
    if (x11_put_image(&c, pid, gc, 0, y, c.w, rows, row) < 0) {
      fprintf(stderr, "wall: PutImage 失败\n");
      return 1;
    }
  }
  for (band = 0; band < copy_h; band += BAND_ROWS) {
    int rows = (copy_h - band < BAND_ROWS) ? (copy_h - band) : BAND_ROWS;
    for (y = 0; y < rows; y++) {
      int sy = topdown ? (src_y + band + y) : (img_h - 1 - (src_y + band + y));
      const uint8_t* s =
          filebuf + b.off + (uint32_t)sy * b.stride + (uint32_t)src_x * bpp;
      uint8_t* d = row + (size_t)y * (size_t)c.w * 4u;
      int x;
      for (x = 0; x < dst_x; x++, d += 4) {
        d[0] = d[1] = d[2] = d[3] = 0;
      }
      for (x = 0; x < copy_w; x++, d += 4, s += bpp) {
        d[0] = s[0]; /* BMP BGR(A) 与 32bpp 屏帧 BGRX 字节序一致 */
        d[1] = s[1];
        d[2] = s[2];
        d[3] = 0;
      }
      for (x = dst_x + copy_w; x < c.w; x++, d += 4) {
        d[0] = d[1] = d[2] = d[3] = 0;
      }
    }
    if (x11_put_image(&c, pid, gc, 0, dst_y + band, c.w, rows, row) < 0) {
      fprintf(stderr, "wall: PutImage 失败\n");
      return 1;
    }
  }
  free(row);

  if (x11_set_root_bg(&c, pid) < 0) {
    fprintf(stderr, "wall: 设置根窗口背景失败\n");
    return 1;
  }
  x11_clear(&c, c.root); /* 触发整屏重绘 */
  x11_sync(&c);
  printf("wall: done\n");
  x11_close(&c);
  return 0;
}
