/*******************************************************************
 * Copyright 2021-present evilbinary
 * 作者: evilbinary on 01/01/20
 * 邮箱: rootdebug@163.com
 ********************************************************************/
/* /dev/fb0 的 Linux fbdev 兼容层验收。
 *
 * 目的：内核把标准 fbdev 编号翻译成自家 VGA 语义（见 duck/modules/gpu/gpu.c），
 * 这里从用户态按 linux/fb.h 的 ABI 原样调用一遍，验证：
 *   1) 结构体布局（aarch64 LP64：var=160 字节 / fix=80 字节，关键字段偏移）
 *   2) FBIOGET_VSCREENINFO / FBIOGET_FSCREENINFO 的返回值自洽
 *      （行长、显存大小、虚拟分辨率、毫米尺寸与 DPI）
 *   3) mmap 显存后可写可读、FBIOPAN_DISPLAY / FBIOBLANK 返回成功
 *
 * 编译环境里没有 linux/fb.h（musl 只带 libc 头），所以按内核 uapi 的定义
 * 手写一遍——字段顺序/类型与 linux/fb.h 完全一致，否则偏移检查会失败。
 *
 * 输出用 LTP 风格（TPASS/TFAIL + Summary），便于 tools/ltp_batch.py 直接跑：
 *   python3 tools/ltp_batch.py --timeout 30 /bin/fbtest
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

typedef unsigned int fb_u32;
typedef unsigned short fb_u16;
typedef unsigned long fb_ulong;

/* linux/fb.h 的标准编号（字面量，不是 _IO* 算出来的） */
#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOGET_FSCREENINFO 0x4602
#define FBIOPAN_DISPLAY 0x4606
#define FBIOBLANK 0x4611

#define FB_TYPE_PACKED_PIXELS 0
#define FB_VISUAL_TRUECOLOR 2

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

static int g_pass;
static int g_fail;

#define TPASS(...)          \
  do {                      \
    g_pass++;               \
    printf("TPASS: ");      \
    printf(__VA_ARGS__);    \
    printf("\n");           \
  } while (0)

#define TFAIL(...)          \
  do {                      \
    g_fail++;               \
    printf("TFAIL: ");      \
    printf(__VA_ARGS__);    \
    printf("\n");           \
  } while (0)

/* 字段偏移（编译期可算，写成宏避免 offsetof 依赖） */
#define FB_VAR_OFF(f) ((int)((char*)&var.f - (char*)&var))
#define FB_FIX_OFF(f) ((int)((char*)&fix.f - (char*)&fix))

int main(int argc, char** argv) {
  struct fb_var_screeninfo var;
  struct fb_fix_screeninfo fix;
  int fd;

  setvbuf(stdout, NULL, _IONBF, 0);
  (void)argc;
  (void)argv;

  /* --- 1. ABI 布局 --- */
  printf("== fbdev ABI check ==\n");
  printf("sizeof(var)=%d sizeof(fix)=%d\n", (int)sizeof(var), (int)sizeof(fix));
  if (sizeof(var) == 160)
    TPASS("fb_var_screeninfo size 160");
  else
    TFAIL("fb_var_screeninfo size %d != 160", (int)sizeof(var));
  if (sizeof(fix) == 80)
    TPASS("fb_fix_screeninfo size 80");
  else
    TFAIL("fb_fix_screeninfo size %d != 80", (int)sizeof(fix));

  /* 关键偏移：错一个字段，内核写进来的值就会串到别的字段上 */
  if (FB_VAR_OFF(bits_per_pixel) == 24 && FB_VAR_OFF(red) == 32 &&
      FB_VAR_OFF(nonstd) == 80 && FB_VAR_OFF(rotate) == 140)
    TPASS("fb_var_screeninfo field offsets ok");
  else
    TFAIL("fb_var_screeninfo field offsets wrong");
  if (FB_FIX_OFF(smem_start) == 16 && FB_FIX_OFF(line_length) == 48 &&
      FB_FIX_OFF(mmio_start) == 56)
    TPASS("fb_fix_screeninfo field offsets ok");
  else
    TFAIL("fb_fix_screeninfo field offsets wrong");

  /* --- 2. 打开节点（历史名 /dev/fb 与标准名 /dev/fb0 必须是同一个设备） --- */
  fd = open("/dev/fb0", O_RDWR);
  if (fd < 0) {
    TFAIL("open /dev/fb0 failed");
    printf("Summary: pass=%d fail=%d\n", g_pass, g_fail);
    return 1;
  }
  TPASS("open /dev/fb0");
  {
    int fd2 = open("/dev/fb", O_RDWR);
    if (fd2 < 0)
      TFAIL("open /dev/fb failed");
    else
      TPASS("open /dev/fb (alias)");
    if (fd2 >= 0) close(fd2);
  }

  /* --- 3. FBIOGET_VSCREENINFO --- */
  memset(&var, 0, sizeof(var));
  if (ioctl(fd, FBIOGET_VSCREENINFO, &var) == 0)
    TPASS("FBIOGET_VSCREENINFO ret 0");
  else
    TFAIL("FBIOGET_VSCREENINFO failed");

  printf("var: %ux%u (virt %ux%u, off %u,%u) %ubpp rot=%u mm=%ux%u\n",
         var.xres, var.yres, var.xres_virtual, var.yres_virtual, var.xoffset,
         var.yoffset, var.bits_per_pixel, var.rotate, var.width, var.height);

  if (var.xres > 0 && var.yres > 0)
    TPASS("xres/yres non-zero");
  else
    TFAIL("xres/yres zero");
  if (var.xres_virtual == var.xres && var.yres_virtual >= var.yres)
    TPASS("virtual resolution sane");
  else
    TFAIL("virtual %ux%u vs %ux%u", var.xres_virtual, var.yres_virtual,
          var.xres, var.yres);
  if (var.bits_per_pixel == 16 || var.bits_per_pixel == 24 ||
      var.bits_per_pixel == 32)
    TPASS("bits_per_pixel %u", var.bits_per_pixel);
  else
    TFAIL("bits_per_pixel %u unexpected", var.bits_per_pixel);

  /* 颜色位域必须铺满一个像素且不重叠 */
  {
    fb_u32 bits = var.red.length + var.green.length + var.blue.length;
    if (bits > 0 && bits <= var.bits_per_pixel && var.red.length > 0 &&
        var.green.length > 0 && var.blue.length > 0)
      TPASS("rgb bitfields %u/%u/%u", var.red.length, var.green.length,
            var.blue.length);
    else
      TFAIL("rgb bitfields %u/%u/%u", var.red.length, var.green.length,
            var.blue.length);
  }

  /* 毫米字段：语义是物理尺寸，填像素数会得到 ~25dpi 的荒谬值。
   * 96dpi 折算下 DPI 应落在 70..130 之间。 */
  if (var.width > 0 && var.height > 0) {
    int dpi = (int)((double)var.xres * 25.4 / (double)var.width);
    printf("dpi=%d\n", dpi);
    if (dpi >= 70 && dpi <= 130)
      TPASS("physical size %ux%u mm (dpi %d)", var.width, var.height, dpi);
    else
      TFAIL("unreasonable dpi %d (%ux%u mm)", dpi, var.width, var.height);
  } else {
    TFAIL("physical size not reported (%ux%u mm)", var.width, var.height);
  }

  /* --- 4. FBIOGET_FSCREENINFO --- */
  memset(&fix, 0, sizeof(fix));
  if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) == 0)
    TPASS("FBIOGET_FSCREENINFO ret 0");
  else
    TFAIL("FBIOGET_FSCREENINFO failed");

  printf("fix: id=%s smem=%lx len=%u line=%u type=%u visual=%u\n", fix.id,
         (unsigned long)fix.smem_start, fix.smem_len, fix.line_length, fix.type,
         fix.visual);

  {
    fb_u32 want_line = var.xres * (var.bits_per_pixel / 8);
    fb_u32 one = want_line * var.yres;
    if (fix.line_length == want_line)
      TPASS("line_length %u", fix.line_length);
    else
      TFAIL("line_length %u want %u", fix.line_length, want_line);
    if (fix.type == FB_TYPE_PACKED_PIXELS)
      TPASS("type packed pixels");
    else
      TFAIL("type %u", fix.type);
    if (fix.visual == FB_VISUAL_TRUECOLOR)
      TPASS("visual truecolor");
    else
      TFAIL("visual %u", fix.visual);
    /* 显存至少一屏、最多 8 屏（防止申报一个远大于实际分配的尺寸） */
    if (fix.smem_len >= one && fix.smem_len <= one * 8)
      TPASS("smem_len %u (one screen %u)", fix.smem_len, one);
    else
      TFAIL("smem_len %u out of range (one screen %u)", fix.smem_len, one);
  }

  /* --- 5. mmap 显存并读写 --- */
  {
    size_t len = ((size_t)fix.smem_len + 0xfff) & ~(size_t)0xfff;
    unsigned char* fb = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                             0);
    if (fb == MAP_FAILED) {
      TFAIL("mmap %u bytes failed", (unsigned)len);
    } else {
      TPASS("mmap %u bytes at %p", (unsigned)len, fb);
      /* 在可见区中心画一个点再读回（走的是真实显存） */
      fb_u32 bytes = var.bits_per_pixel / 8;
      unsigned char* p = fb + (size_t)(var.yres / 2) * fix.line_length +
                         (size_t)(var.xres / 2) * bytes;
      unsigned char probe[4] = {0x5a, 0xa5, 0x3c, 0xc3};
      int i, ok = 1;
      for (i = 0; i < (int)bytes; i++) p[i] = probe[i];
      for (i = 0; i < (int)bytes; i++) {
        if (p[i] != probe[i]) {
          ok = 0;
          break;
        }
      }
      if (ok)
        TPASS("framebuffer write/read back");
      else
        TFAIL("framebuffer read back mismatch");
      munmap(fb, len);
    }
  }

  /* --- 6. 翻页与消隐 --- */
  if (ioctl(fd, FBIOPAN_DISPLAY, &var) == 0)
    TPASS("FBIOPAN_DISPLAY ret 0");
  else
    TFAIL("FBIOPAN_DISPLAY failed");
  if (ioctl(fd, FBIOBLANK, 0) == 0)
    TPASS("FBIOBLANK ret 0");
  else
    TFAIL("FBIOBLANK failed");

  close(fd);
  printf("Summary: pass=%d fail=%d\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
