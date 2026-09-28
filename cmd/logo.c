/*******************************************************************
 * Copyright 2021-present evilbinary
 * 作者: evilbinary on 01/01/20
 * 邮箱: rootdebug@163.com
 *
 * YiYiYa Console OS 启动动画 /bin/logo
 *
 * 由 /bin/config 读 /conf/init.conf 的第一行启动：
 *     wait /bin/logo
 * config 会等它播完再启动 /bin/ymain（见 app/cmd/config.c 的 "wait " 前缀）。
 *
 * 画面：纯黑底 + 设计稿 app/resource/res/logo.png（已抠底透明 + 裁剪 + 缩放到
 * 142x34，见 /tmp/opencode/trim_logo.py）。无星场、无进度条 —— 只有 logo 和
 * 一道从文字头部扫到尾部的高光。
 *
 * 【不放大 ⇒ 无锯齿】图片按 142x34 生成，640 屏时 fw = W*2/9 = 142 ⇒ 1:1 绘制
 * （逐像素直拷 + alpha 混合）。只有屏幕比图片还小时才盒式平均缩小。
 *
 * 【不闪屏】screen_flush 每帧要 clean 1.2MB dcache（QEMU 上约 360ms），整屏重绘
 * 会闪。所以：黑底只铺一次；高光扫过时【只重画 logo 那一小块】（背景是纯黑，
 * 直接填黑即可，无拖影），从不整屏重绘。
 ********************************************************************/
#include "screen.h"
#include "xwin_user.h"

#include "image.h"
#include "png_decoder.h"

#include "stdio.h"
#include "string.h"
#include "time.h"

/* ---- 可调参数 ---------------------------------------------------- */
#define STAGE_MS 180      /* 每步之间停一下(ms)：8 步 ≈ 1.5s，能在 ymain 建窗口前播完 */
#define LOGO_DIAG 0       /* 打印每步实际等待时长（定位"闪一下"用，稳定后关掉）*/
#define LOGO_PATH "/res/logo.png"
#define LOGO_MAX_MS 30000 /* 仅安全兜底，正常不会触发 */

#define BEAM_W 30   /* 高光束宽度(px) */
#define BEAM_ADD 130 /* 高光提亮量（0..255）*/
#define BEAM_STEPS 6 /* 从头部扫到尾部分几步 */

/* ---- 全局 -------------------------------------------------------- */
static screen_info_t* g_sc = NULL;
static bitmap_t* g_logo = NULL;
static unsigned char* g_src = NULL; /* RGBA 字节流 */
static int g_bw = 0, g_bh = 0;

/* 延时 ms。
 * 【为什么不用 nanosleep】miyoo 真机上内核的 nanosleep 基本不生效 —— 实测整个
 * 启动动画（8 步）只花了 137ms，等于一闪而过。所以这里以 xwin_get_ticks()
 * （毫秒 @1kHz，见 duck/kernel/schedule.c）为准自己做墙钟等待；nanosleep 只用来
 * 让出 CPU（在 QEMU/versatilepb 上它才真正生效）。带迭代上限，绝不挂死。 */
static void logo_sleep_ms(int ms) {
  u32 t0, t1;
  int guard;
  if (ms <= 0) return;
  t0 = xwin_get_ticks();
  for (guard = 0; guard < 20000; guard++) {
    struct timespec ts;
    if ((u32)(xwin_get_ticks() - t0) >= (u32)ms) break;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000L; /* 1ms */
    nanosleep(&ts, NULL);
  }
  t1 = xwin_get_ticks();
  /* 诊断：每步实际等了多久。若这里恒为 0 而日志里 "logo: done" 也很小，
     说明 xwin_get_ticks() 在这个线程上不前进（SMP 迁移/tick 问题），
     那就得改内核侧（让 get_ticks 返回固定 CPU 的计数）。 */
  if (LOGO_DIAG) {
    printf("logo: sleep %dms -> %ums (t=%u)\n", ms, (unsigned)(t1 - t0),
           (unsigned)t1);
  }
}

/* 把 (r,g,b,a) 按 alpha 混到 (X,Y) 上 */
static void blend_px(int X, int Y, int r, int g, int b, int a) {
  u32* buf;
  u32 d;
  int dr, dg, db;
  if (g_sc == NULL || X < 0 || Y < 0 || X >= g_sc->width || Y >= g_sc->height) {
    return;
  }
  if (a <= 0) return;
  if (a > 255) a = 255;
  buf = g_sc->buffer;
  d = buf[Y * g_sc->width + X];
  dr = (int)((d >> 16) & 0xFF);
  dg = (int)((d >> 8) & 0xFF);
  db = (int)(d & 0xFF);
  dr = (r * a + dr * (255 - a)) / 255;
  dg = (g * a + dg * (255 - a)) / 255;
  db = (b * a + db * (255 - a)) / 255;
  buf[Y * g_sc->width + X] =
      0xFF000000u | ((u32)dr << 16) | ((u32)dg << 8) | (u32)db;
}

/* logo 在屏幕上的落点（居中，略高于正中）*/
static void logo_pos(int dw, int dh, int* px, int* py) {
  *px = (g_sc->width - dw) / 2;
  *py = (g_sc->height - dh) / 2 - g_sc->height / 24;
}

/* 把 logo 画到 (dw x dh)，bright=0..100 为整体亮度；
 * beam_x>=0 时叠加一道竖直高光（|X-beam_x| < BEAM_W）。 */
static void logo_draw(int dw, int dh, int bright, int beam_x) {
  int W, H, dx, dy, j, i;
  if (g_src == NULL || g_sc == NULL || dw <= 0 || dh <= 0) return;
  W = g_sc->width;
  H = g_sc->height;
  logo_pos(dw, dh, &dx, &dy);

  if (dw >= g_bw || dh >= g_bh) {
    /* ---- 1:1（不缩放，无锯齿）---- */
    for (j = 0; j < g_bh && j < dh; j++) {
      int Y = dy + j;
      const unsigned char* row;
      if (Y < 0 || Y >= H) continue;
      row = g_src + (size_t)j * g_bw * 4;
      for (i = 0; i < g_bw && i < dw; i++) {
        int X = dx + i;
        const unsigned char* p = row + i * 4;
        int r, g, b, a, add = 0;
        if (X < 0 || X >= W || p[3] == 0) continue;
        r = p[0];
        g = p[1];
        b = p[2];
        a = p[3];
        if (beam_x >= 0) {
          int dd = X - beam_x;
          if (dd < 0) dd = -dd;
          if (dd < BEAM_W) add = (BEAM_W - dd) * BEAM_ADD / BEAM_W;
        }
        r += add;
        g += add;
        b += add;
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        blend_px(X, Y, r, g, b, a * bright / 100);
      }
    }
    return;
  }

  /* ---- 缩小：盒式平均（alpha 加权 ⇒ 不会发灰/发暗）---- */
  for (j = 0; j < dh; j++) {
    int sy0 = j * g_bh / dh;
    int sy1 = (j + 1) * g_bh / dh;
    int Y = dy + j;
    if (sy1 <= sy0) sy1 = sy0 + 1;
    if (sy1 > g_bh) sy1 = g_bh;
    if (Y < 0 || Y >= H) continue;
    for (i = 0; i < dw; i++) {
      int sx0 = i * g_bw / dw;
      int sx1 = (i + 1) * g_bw / dw;
      int X = dx + i;
      int sr = 0, sg = 0, sb = 0, sa = 0, n = 0, sy, sx;
      if (sx1 <= sx0) sx1 = sx0 + 1;
      if (sx1 > g_bw) sx1 = g_bw;
      if (X < 0 || X >= W) continue;
      for (sy = sy0; sy < sy1; sy++) {
        const unsigned char* row = g_src + (size_t)sy * g_bw * 4;
        for (sx = sx0; sx < sx1; sx++) {
          const unsigned char* p = row + sx * 4;
          int a = p[3];
          sr += p[0] * a;
          sg += p[1] * a;
          sb += p[2] * a;
          sa += a;
          n++;
        }
      }
      if (n == 0 || sa == 0) continue;
      /* sr/sa = sum(r*a)/sum(a) = 解预乘后的颜色 */
      blend_px(X, Y, sr / sa, sg / sa, sb / sa, (sa / n) * bright / 100);
    }
  }
}

int main(int argc, char* argv[]) {
  int W, H, fw, fh, lx, ly, i;
  u32 t0;

  (void)argc;
  (void)argv;

  screen_init();
  g_sc = screen_info();
  if (g_sc == NULL || g_sc->buffer == NULL || g_sc->width <= 0 ||
      g_sc->height <= 0) {
    printf("logo: screen not ready, skip animation\n");
    return 0;
  }
  W = g_sc->width;
  H = g_sc->height;
  printf("logo: start %dx%d\n", W, H);

  /* 载入设计稿。注意 load_png() 打开失败时 return 1（不是 NULL），
     所以这里连指针大小一起判断，避免拿到假指针去解引用。 */
  g_logo = load_png(LOGO_PATH);
  if ((unsigned long)g_logo < 4096 || g_logo == NULL || g_logo->w <= 0 ||
      g_logo->h <= 0 || g_logo->bits == NULL) {
    printf("logo: load %s failed, fallback text\n", LOGO_PATH);
    g_logo = NULL;
  }
  if (g_logo != NULL) {
    g_src = (unsigned char*)g_logo->bits;
    g_bw = g_logo->w;
    g_bh = g_logo->h;
    printf("logo: image %dx%d %dbpp alpha=%d\n", g_bw, g_bh,
           g_logo->bitperpixel, g_logo->alpha_flag);
  }

  t0 = xwin_get_ticks();

  /* 显示尺寸：屏宽约 2/9（640 → 142）—— 比原来的 1/3 再小 1/3。
     图片就是按 142 宽生成的，640 屏上正好 1:1。
     图片比目标还小时取原尺寸 —— 【绝不放大】⇒ 没有锯齿。 */
  fw = W * 2 / 9;
  if (g_bw > 0 && fw > g_bw) fw = g_bw;
  if (fw < 8) fw = 8;
  fh = (g_bh > 0 && g_bw > 0) ? fw * g_bh / g_bw : fw * 34 / 142;
  if (fh < 8) fh = 8;
  if (g_bh > 0 && fh > g_bh) fh = g_bh;

  /* ---- 1) 纯黑底（整个动画只铺这一次；没有星场、没有进度条）---- */
  screen_fill_rect(0, 0, W, H, 0x000000);
  if (g_logo != NULL) {
    logo_draw(fw, fh, 100, -1);
  } else {
    screen_draw_string_with_color((W - 9 * 8) / 2, H / 2 - 8, (i8*)"YiYiYa OS",
                                  0xFFFFFF, 0);
    screen_flush();
    logo_sleep_ms(400);
    goto done;
  }
  screen_flush();
  logo_sleep_ms(STAGE_MS);

  if (xwin_get_ticks() - t0 > LOGO_MAX_MS) goto done;

  /* ---- 2) 一道光：从文字头部扫到尾部 ----
   * 每一步只重画 logo 那一小块（背景纯黑，先填黑恢复即可，无拖影），
   * 不整屏重绘 ⇒ 不闪。 */
  logo_pos(fw, fh, &lx, &ly);
  for (i = 0; i < BEAM_STEPS; i++) {
    int bx;
    if (BEAM_STEPS > 1) {
      bx = lx - BEAM_W + (fw + 2 * BEAM_W) * i / (BEAM_STEPS - 1);
    } else {
      bx = lx + fw / 2;
    }
    screen_fill_rect(lx, ly, fw, fh, 0x000000); /* 只恢复 logo 区域 */
    logo_draw(fw, fh, 100, bx);
    screen_flush();
    logo_sleep_ms(STAGE_MS);
    if (xwin_get_ticks() - t0 > LOGO_MAX_MS) break;
  }

  /* 收尾：擦掉高光，留下干净的 logo */
  screen_fill_rect(lx, ly, fw, fh, 0x000000);
  logo_draw(fw, fh, 100, -1);
  screen_flush();

done:
  /* 退出前销毁自己的 xwin 窗口。libgui 自己换模式时就是这么做的
     （screen.c 里 screen_set_mode/screen_set_size 都调 xwin_destroy）。
     不销毁的话 kernel 里会残留一个全屏 DIRECT 窗口，紧接着启动的
     /bin/ymain 创建窗口时可能撞上，表现为 ymain 一起来就崩。 */
  if (g_sc != NULL && g_sc->xwin_handle != 0) {
    xwin_destroy(g_sc->xwin_handle);
    g_sc->xwin_handle = 0;
  }
  printf("logo: done %ums\n", (unsigned)(xwin_get_ticks() - t0));
  return 0;
}
