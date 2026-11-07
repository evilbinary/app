/*
 * fbsample：读 /dev/fb0 采样，确认 weston 是否真的把画面写进了 framebuffer。
 * 输出几个关键像素（RGB565）和 4x4x4 粗直方图。
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

typedef unsigned int fb_u32;
typedef unsigned short fb_u16;
typedef unsigned long fb_ulong;

#define FBIOGET_VSCREENINFO 0x4600
#define FBIOGET_FSCREENINFO 0x4602

struct fb_bitfield {
  fb_u32 offset;
  fb_u32 length;
  fb_u32 msb_right;
};

struct fb_var_screeninfo {
  fb_u32 xres;
  fb_u32 yres;
  fb_u32 xres_virtual;
  fb_u32 yres_virtual;
  fb_u32 xoffset;
  fb_u32 yoffset;
  fb_u32 bits_per_pixel;
  fb_u32 grayscale;
  struct fb_bitfield red;
  struct fb_bitfield green;
  struct fb_bitfield blue;
  struct fb_bitfield transp;
  fb_u32 nonstd;
  fb_u32 activate;
  fb_u32 height;
  fb_u32 width;
  fb_u32 accel_flags;
  fb_u32 pixclock;
  fb_u32 left_margin;
  fb_u32 right_margin;
  fb_u32 upper_margin;
  fb_u32 lower_margin;
  fb_u32 hsync_len;
  fb_u32 vsync_len;
  fb_u32 sync;
  fb_u32 vmode;
  fb_u32 rotate;
  fb_u32 colorspace;
  fb_u32 reserved[4];
};

struct fb_fix_screeninfo {
  char id[16];
  fb_ulong smem_start;
  fb_u32 smem_len;
  fb_u32 type;
  fb_u32 type_aux;
  fb_u32 visual;
  fb_u16 xpanstep;
  fb_u16 ypanstep;
  fb_u16 ywrapstep;
  fb_u32 line_length;
  fb_ulong mmio_start;
  fb_u32 mmio_len;
  fb_u32 accel;
  fb_u16 capabilities;
  fb_u16 reserved[2];
};

int main(void) {
  struct fb_var_screeninfo var;
  struct fb_fix_screeninfo fix;
  unsigned short *fb;
  unsigned int hist[256];
  unsigned int total = 0;
  unsigned int y, x;
  size_t maplen;
  int fd;
  int i;

  fd = open("/dev/fb0", O_RDWR);
  if (fd < 0) {
    perror("open /dev/fb0");
    return 1;
  }
  memset(&var, 0, sizeof(var));
  memset(&fix, 0, sizeof(fix));
  if (ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0 ||
      ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0) {
    perror("FBIOGET");
    close(fd);
    return 1;
  }
  printf("fb: %ux%u bpp=%u stride=%u smem_len=%u\n", var.xres, var.yres,
         var.bits_per_pixel, fix.line_length, fix.smem_len);

  maplen = (size_t)fix.line_length * var.yres;
  fb = mmap(NULL, maplen, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (fb == MAP_FAILED) {
    perror("mmap /dev/fb0");
    close(fd);
    return 1;
  }

  for (i = 0, y = 0; y < var.yres; y += 16) {
    for (x = 0; x < var.xres; x += 16) {
      unsigned short p = fb[(size_t)y * (fix.line_length / 2) + x];
      unsigned int r = (p >> 11) & 31;
      unsigned int g = (p >> 5) & 63;
      unsigned int b = p & 31;
      hist[(r >> 2) * 32 + (g >> 4) * 8 + (b >> 2)]++;
      total++;
    }
  }

  printf("pixels: p00=%04x p10_10=%04x pcenter=%04x pbottom=%04x\n",
         fb[0], fb[(size_t)10 * (fix.line_length / 2) + 10],
         fb[(size_t)(var.yres / 2) * (fix.line_length / 2) + var.xres / 2],
         fb[(size_t)(var.yres - 1) * (fix.line_length / 2) + var.xres / 2]);

  printf("top colors (r3 g2 b3 bins):\n");
  for (i = 0; i < 256; i++) {
    if (hist[i] == 0)
      continue;
    printf("  bin %d: %u / %u (%u%%)\n", i, hist[i], total,
           hist[i] * 100 / total);
  }

  munmap(fb, maplen);
  close(fd);
  return 0;
}
