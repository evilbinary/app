/*******************************************************************
 * xeyes — 自研"跟着指针看的眼睛"（替代 Xorg 自带的 xeyes）。
 *
 * 为什么不用原生 xeyes：它每来一条 MotionNotify 就把整块眼睛重画一遍，
 * 在软件渲染的宿主 X server（XQuartz）上，大面积重画就是掉帧的主因。
 * 这里三处控制成本：
 *   1. 指针位置用 QueryPointer 每 30ms 采一次（一次往返约 0.05ms），
 *      不订阅 MotionNotify —— 事件流里少一大票 32 字节事件。
 *   2. 瞳孔只在"真的移动了"时重画，而且只重画动了的那只眼睛。
 *   3. 眼睛是三层椭圆（眼眶/眼白/瞳孔），全部走服务器端 PolyFillArc，
 *      客户端一个像素都不碰（不用 PutImage）。
 *
 * 用法：xeyes [WxH[+X+Y]]
 ********************************************************************/
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "x11.h"

#define SAMPLE_MS 30 /* 指针采样周期：33Hz，眼睛看不出阶梯 */

#define BG X11_RGB(0x1e, 0x22, 0x2a)
#define EDGE X11_RGB(0x0a, 0x0c, 0x10)
#define SCLERA X11_RGB(0xff, 0xff, 0xff)
#define PUPIL X11_RGB(0x12, 0x14, 0x1a)

static x11_t c;
static uint32_t win, gc;
static int win_w = 200, win_h = 160;
static int eye_w, eye_h, eye_cy, eye_cx[2], pupil_r;
static int pupil_x[2] = {-1, -1}, pupil_y[2] = {-1, -1};
static int ptr_x, ptr_y;

static void layout(void) {
  eye_w = win_w * 34 / 100;
  eye_h = win_h * 64 / 100;
  if (eye_w < 16) {
    eye_w = 16;
  }
  if (eye_h < 16) {
    eye_h = 16;
  }
  eye_cy = win_h / 2;
  eye_cx[0] = win_w * 30 / 100;
  eye_cx[1] = win_w * 70 / 100;
  pupil_r = (eye_w < eye_h ? eye_w : eye_h) * 26 / 100;
  if (pupil_r < 4) {
    pupil_r = 4;
  }
}

/* 瞳孔目标位置：把"眼睛中心→指针"的方向映射到眼白内圈。
 * 用 max 范数代替开方归一化（省掉 libm，视觉上分辨不出来）。 */
static void pupil_target(int i, int px, int py, int* ox, int* oy) {
  int dx = px - eye_cx[i];
  int dy = py - eye_cy;
  int ax = dx < 0 ? -dx : dx;
  int ay = dy < 0 ? -dy : dy;
  int m = ax > ay ? ax : ay;
  int rx = eye_w / 2 - pupil_r - 3;
  int ry = eye_h / 2 - pupil_r - 3;
  int nx = eye_cx[i];
  int ny = eye_cy;
  if (rx < 0) {
    rx = 0;
  }
  if (ry < 0) {
    ry = 0;
  }
  if (m > 0) {
    nx += dx * rx / m;
    ny += dy * ry / m;
  }
  *ox = nx;
  *oy = ny;
}

static void draw_eye(int i) {
  int ex = eye_cx[i] - eye_w / 2;
  int ey = eye_cy - eye_h / 2;
  int px = pupil_x[i] - pupil_r;
  int py = pupil_y[i] - pupil_r;
  /* 眼眶：比眼白大 3px 的深色椭圆（省掉描边请求，两层填充就够） */
  x11_set_gc(&c, gc, 0x0C /* FG|BG */, EDGE, EDGE);
  x11_fill_ellipse(&c, win, gc, ex - 3, ey - 3, eye_w + 6, eye_h + 6);
  x11_set_gc(&c, gc, 0x0C, SCLERA, SCLERA);
  x11_fill_ellipse(&c, win, gc, ex, ey, eye_w, eye_h);
  x11_set_gc(&c, gc, 0x0C, PUPIL, PUPIL);
  x11_fill_ellipse(&c, win, gc, px, py, pupil_r * 2, pupil_r * 2);
}

/* 跟着指针走；animate=0 时只更新坐标不画（整窗重绘用） */
static void follow(int px, int py, int animate) {
  int i;
  for (i = 0; i < 2; i++) {
    int nx, ny;
    pupil_target(i, px, py, &nx, &ny);
    if (nx != pupil_x[i] || ny != pupil_y[i]) {
      pupil_x[i] = nx;
      pupil_y[i] = ny;
      if (animate) {
        draw_eye(i);
      }
    }
  }
}

static void draw_all(void) {
  x11_clear(&c, win); /* 窗口 bg 已设成 BG：ClearArea 由服务器填色 */
  follow(ptr_x, ptr_y, 0);
  draw_eye(0);
  draw_eye(1);
}

int main(int argc, char* argv[]) {
  uint8_t ev[32];
  int w = 200, h = 160, gx = -1, gy = -1, x, y;
  uint32_t del_type = 0, del_data = 0;
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  if (argc > 1) {
    (void)x11_parse_geom(argv[1], &w, &h, &gx, &gy);
  }
  if (x11_connect(&c, NULL) < 0) {
    return 1;
  }
  /* 单独跑（不起 wm）时屏幕上也要有指针 */
  x11_install_arrow_cursor(&c);
  gc = x11_id(&c);
  if (x11_create_gc(&c, gc, c.root) < 0) {
    fprintf(stderr, "xeyes: CreateGC 失败\n");
    return 1;
  }

  win_w = w;
  win_h = h;
  layout();
  x = (gx >= 0) ? gx : (c.w - w - 60);
  y = (gy >= 0) ? gy : (c.h / 3);
  if (x < 0) {
    x = 0;
  }
  if (y < 0) {
    y = 0;
  }
  /* override_redirect=0：让 wm 给它加标题栏（和原生客户端一样是"受管窗口"） */
  win = x11_create_window(&c, x, y, w, h, BG, 0,
                          X11_EV_EXPOSURE | X11_EV_STRUCTURE);
  if (win == 0) {
    fprintf(stderr, "xeyes: 建窗口失败\n");
    return 1;
  }
  x11_set_title(&c, win, "xeyes");
  (void)x11_enable_delete_window(&c, win, &del_type, &del_data);
  x11_map_window(&c, win);
  printf("xeyes: %dx%d at %d,%d\n", w, h, x, y);

  ptr_x = w / 2;
  ptr_y = h / 2;
  (void)x11_query_pointer(&c, win, NULL, NULL, &ptr_x, &ptr_y, NULL);
  draw_all();

  for (;;) {
    struct pollfd pfd;
    int r;
    int quit = 0;
    pfd.fd = c.fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, SAMPLE_MS) > 0) {
      int got = 0;
      while ((r = x11_next_event(&c, ev)) == 1) {
        got++;
        /* 事件 type 最高位是 send_event 标志（wm 发的关闭请求就是这样），
         * 比对类型前一律先掩掉 */
        if ((ev[0] & 0x7f) == X11_CLIENT_MESSAGE && del_type != 0 &&
            x11_ev_u32(ev, 8) == del_type && x11_ev_u32(ev, 12) == del_data) {
          quit = 1; /* wm 点了 × */
          break;
        }
        if (ev[0] == X11_EXPOSE || ev[0] == X11_MAP_NOTIFY) {
          /* MapNotify 也重绘：wm reparent 的 unmap→map 期间窗口内容可能被
           * 服务器丢掉，而它不一定补 Expose（backing store 开着时就这样，
           * 实测整窗空白）。MAP 事件重绘一次最省事。 */
          if (x11_ev_u32(ev, 4) == win) {
            draw_all();
          }
        } else if (ev[0] == X11_CONFIGURE_NOTIFY) {
          /* wm 最大化/缩放后按新尺寸重排眼睛 */
          int nw = (int)ev[20] | ((int)ev[21] << 8);
          int nh = (int)ev[22] | ((int)ev[23] << 8);
          if (nw > 0 && nh > 0 && (nw != win_w || nh != win_h)) {
            win_w = nw;
            win_h = nh;
            layout();
            draw_all();
          }
        }
      }
      if (quit || r < 0) {
        break; /* 关闭请求 / 连接断了（XKillClient）：退出进程 */
      }
      /* 假唤醒（报就绪却读不到事件）让一步，别把 CPU 烧在空转上 */
      if (got == 0) {
        usleep(2000);
      }
    }
    /* 指针位置统一由这里跟进：指针移出窗口后 MotionNotify 不会来了 */
    {
      int px, py;
      if (x11_query_pointer(&c, win, NULL, NULL, &px, &py, NULL) == 0) {
        if (px != ptr_x || py != ptr_y) {
          ptr_x = px;
          ptr_y = py;
          follow(px, py, 1);
        }
      }
    }
  }
  return 0;
}
