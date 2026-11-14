/*
 * fbshot：把 /dev/fb0 当前画面 dump 成 PPM (P6)。
 *
 *   fbshot [输出文件]        默认 /tmp/shot.ppm
 *
 * fbdev 是 RGB565；转成 RGB888 写 PPM。Mac 端 tools/screenshot.sh
 * 会拉回来并转 PNG。
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
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

int main(int argc, char** argv) {
  const char* out = (argc > 1) ? argv[1] : "/tmp/shot.ppm";
  struct fb_var_screeninfo var;
  struct fb_fix_screeninfo fix;
  const unsigned short* fb;
  unsigned char* row;
  unsigned int x, y;
  size_t maplen;
  int fd;
  FILE* f;

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
  if (var.bits_per_pixel != 16) {
    fprintf(stderr, "only RGB565 (16bpp) supported, got %u bpp\n",
            var.bits_per_pixel);
    close(fd);
    return 1;
  }

  maplen = (size_t)fix.line_length * var.yres;
  fb = mmap(NULL, maplen, PROT_READ, MAP_SHARED, fd, 0);
  if (fb == MAP_FAILED) {
    perror("mmap /dev/fb0");
    close(fd);
    return 1;
  }

  row = malloc((size_t)var.xres * 3);
  if (row == NULL) {
    fprintf(stderr, "out of memory\n");
    munmap((void*)fb, maplen);
    close(fd);
    return 1;
  }

  f = fopen(out, "wb");
  if (f == NULL) {
    perror(out);
    free(row);
    munmap((void*)fb, maplen);
    close(fd);
    return 1;
  }
  fprintf(f, "P6\n%u %u\n255\n", var.xres, var.yres);

  for (y = 0; y < var.yres; y++) {
    const unsigned short* src =
        fb + (size_t)y * (fix.line_length / 2) + var.xoffset;
    for (x = 0; x < var.xres; x++) {
      unsigned short p = src[x];
      unsigned int r = (p >> 11) & 0x1f;
      unsigned int g = (p >> 5) & 0x3f;
      unsigned int b = p & 0x1f;
      row[x * 3 + 0] = (unsigned char)((r << 3) | (r >> 2));
      row[x * 3 + 1] = (unsigned char)((g << 2) | (g >> 4));
      row[x * 3 + 2] = (unsigned char)((b << 3) | (b >> 2));
    }
    fwrite(row, 1, (size_t)var.xres * 3, f);
  }
  fclose(f);
  printf("saved %ux%u -> %s\n", var.xres, var.yres, out);

  free(row);
  munmap((void*)fb, maplen);
  close(fd);
  return 0;
}
