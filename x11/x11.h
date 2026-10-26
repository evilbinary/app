/*******************************************************************
 * x11.h — 极简 X11 客户端协议库（无 libX11 依赖）。
 *
 * 背景：镜像里的 libX11 是 glibc 版，而我们的用户态是 musl；而且
 * feh/hsetroot/pcmanfm/lxpanel 这些现成组件要么不在镜像里，要么依赖
 * libfm/dbus/GTK 拉不动。所以这里直接手写 X11 线上协议（unix socket +
 * 小端请求报文），够自研桌面组件用即可。
 *
 * 已实现（按需增补）：
 *   连接/无鉴权 Setup、资源 id 分配、CreateWindow/MapWindow/DestroyWindow、
 *   属性（背景像素/位图、事件掩码、override-redirect）、CreateGC/ChangeGC、
 *   CreatePixmap、PutImage(ZPixmap)、PolyFillRectangle、OpenFont/ImageText8、
 *   ClearArea、InternAtom/ChangeProperty、事件读取。
 *
 * 约定：所有 width/height 用像素；32bpp 屏帧按 BGRX 排（Xorg fb 在小端
 * aarch64 上就是 B,G,R,X 字节序）。
 ********************************************************************/
#ifndef YIYIYA_APP_X11_H
#define YIYIYA_APP_X11_H

#include <stdint.h>

/* 事件类型（X11 线上值） */
#define X11_EXPOSE 12
#define X11_CONFIGURE_NOTIFY 22
#define X11_CLIENT_MESSAGE 33
#define X11_MAPPING_NOTIFY 34
#define X11_BUTTON_PRESS 4
#define X11_BUTTON_RELEASE 5
#define X11_MOTION_NOTIFY 6

/* 事件掩码（CreateWindow/ChangeWindowAttributes 用） */
#define X11_EV_KEY_PRESS 0x00000001u
#define X11_EV_BUTTON_PRESS 0x00000004u
#define X11_EV_BUTTON_RELEASE 0x00000008u
#define X11_EV_POINTER_MOTION 0x00000040u
#define X11_EV_EXPOSURE 0x00008000u
#define X11_EV_STRUCTURE 0x00020000u

/* It's a pixel value with the same layout as the screen
 * (BGRX on little-endian 32bpp framebuffers). */
#define X11_RGB(r, g, b) \
  ((uint32_t)(r) << 16 | (uint32_t)(g) << 8 | (uint32_t)(b))

/* 预定义 atom */
#define X11_ATOM_STRING 31
#define X11_ATOM_ATOM 4
#define X11_ATOM_CARDINAL 6
#define X11_ATOM_WINDOW 33

/* 常用 EWMH atom 名 */
#define X11_NET_CLIENT_LIST "_NET_CLIENT_LIST"
#define X11_NET_ACTIVE_WINDOW "_NET_ACTIVE_WINDOW"

typedef struct {
  int fd;
  uint32_t root;
  uint32_t root_visual;
  uint32_t root_depth;
  uint32_t cmap;
  int w, h;
  uint32_t id_base;
  uint32_t id_mask;
  uint32_t next_id;
  int shape_op; /* SHAPE 扩展的 major opcode（0 = 不可用） */
  /* 【事件队列（必须有）】X11 是单条流，回复和事件混在一起。等某个请求的
   * 回复时，服务器很可能先塞进来几条事件（ReparentNotify/Expose/ButtonPress/
   * MotionNotify…）。这些事件不能当回复解析（尺寸/属性立刻变垃圾，而且流
   * 从此错位），也不能丢（Expose 丢了窗口就不重绘），只能先排进这里等
   * 主循环取。 */
  uint8_t evq[32 * 32];
  int evq_head, evq_tail;
} x11_t;

/* 连接：path 传 NULL 用 /tmp/.X11-unix/X0；成功返回 0 */
int x11_connect(x11_t* c, const char* path);
void x11_close(x11_t* c);
uint32_t x11_id(x11_t* c);

/* 同步（等一个答复，确保之前的请求已被服务端处理） */
int x11_sync(x11_t* c);

/* 绘制资源 */
int x11_create_pixmap(x11_t* c, uint32_t pid, uint8_t depth, int w, int h);
int x11_free_pixmap(x11_t* c, uint32_t pid);
int x11_create_gc(x11_t* c, uint32_t gc, uint32_t drawable);
int x11_set_gc(x11_t* c, uint32_t gc, uint32_t mask, uint32_t fg, uint32_t bg);
int x11_set_gc_font(x11_t* c, uint32_t gc, uint32_t font);

/* 像素上传：bgrx 是 w*rows*4 字节的 BGRX 图，左上角放在 (dst_x, dst_y) */
int x11_put_image(x11_t* c, uint32_t drawable, uint32_t gc, int dst_x, int dst_y,
                  int w, int rows, const uint8_t* bgrx);

/* 一张图（BGRX 像素，可直接 x11_put_image） */
typedef struct {
  int w, h;
  uint8_t* px;
} x11_image_t;

/* 读 24/32bpp 未压缩 BMP（依次在 /res/、res/ 下找）；成功返回 0 */
int x11_load_bmp(const char* name, x11_image_t* img);

/* 请求 */
int x11_fill_rect(x11_t* c, uint32_t drawable, uint32_t gc, int x, int y,
                  int w, int h);
int x11_clear(x11_t* c, uint32_t drawable);
int x11_set_root_bg(x11_t* c, uint32_t pixmap);

/* 窗口 */
uint32_t x11_create_window(x11_t* c, int x, int y, int w, int h, uint32_t bg,
                           int override_redirect, uint32_t event_mask);
int x11_map_window(x11_t* c, uint32_t win);
int x11_unmap_window(x11_t* c, uint32_t win);
int x11_destroy_window(x11_t* c, uint32_t win);
int x11_set_window_bg(x11_t* c, uint32_t win, uint32_t pixmap_or_pixel);
/* 背景设成 pixmap；传 1(ParentRelative) 就是"跟着父窗口背景走"（窗口未绘制
 * 的地方显示父窗口内容，例如根窗口的壁纸）。 */
int x11_set_window_bg_pixmap(x11_t* c, uint32_t win, uint32_t pixmap);
int x11_select_input(x11_t* c, uint32_t win, uint32_t event_mask);

/* 文本（X 核心字体，Xorg 内置 "fixed" 可省掉字体文件） */
int x11_open_font(x11_t* c, uint32_t fid, const char* name);
int x11_close_font(x11_t* c, uint32_t fid);
int x11_image_text8(x11_t* c, uint32_t drawable, uint32_t gc, int x, int y,
                    const char* s, int len);

/* 指针抓取（弹层菜单用来收"点到外面"的事件） */
int x11_grab_pointer(x11_t* c, uint32_t win, uint32_t event_mask);
int x11_ungrab_pointer(x11_t* c);

/* 光标：X 不会"自动显示"默认指针（取不到核心光标字体时是隐形光标），
 * 需要客户端自己 CreatePixmapCursor 并挂到窗口上。子窗口创建时继承父窗口的
 * cursor，所以 x11_install_arrow_cursor() 设到 root 即可覆盖后续新建的窗口。 */
int x11_create_pixmap_cursor(x11_t* c, uint32_t cid, uint32_t src, uint32_t mask,
                             int hot_x, int hot_y);
int x11_set_window_cursor(x11_t* c, uint32_t win, uint32_t cursor);
/* 造一个 16x16 箭头（黑笔画 + 白描边）并挂到根窗口；成功返回 0 */
int x11_install_arrow_cursor(x11_t* c);

/* 窗口管理器需要的请求 */
int x11_change_attrs(x11_t* c, uint32_t win, uint32_t value_mask, uint32_t value);
/* 传 X11_KEEP 表示该项不改。不能用 -1 当哨兵：窗口可以拖/缩到负坐标
 * （贴着屏幕左边或上边），-1 是合法坐标。 */
#define X11_KEEP (-32768)
int x11_configure(x11_t* c, uint32_t win, int x, int y, int w, int h);
int x11_raise(x11_t* c, uint32_t win);
int x11_reparent(x11_t* c, uint32_t win, uint32_t parent, int x, int y);
int x11_send_event(x11_t* c, uint32_t dst, uint32_t event_mask, const uint8_t ev32[32]);
int x11_set_focus(x11_t* c, uint32_t win);
int x11_add_to_save_set(x11_t* c, uint32_t win);
int x11_kill_client(x11_t* c, uint32_t resource);
int x11_query_children(x11_t* c, uint32_t win, uint32_t* out, int max);
int x11_get_geometry(x11_t* c, uint32_t win, int* x, int* y, int* w, int* h);
int x11_get_attrs(x11_t* c, uint32_t win, int* map_state, int* override_redirect,
                  uint32_t* all_event_masks);
int x11_get_property32(x11_t* c, uint32_t win, uint32_t prop, uint32_t* out, int max);
int x11_get_property_str(x11_t* c, uint32_t win, uint32_t prop, char* out, int max);

/* SHAPE 扩展（圆角/异形窗口）：probe 成功返回 0 并记下扩展 opcode；
 * shape_rects 把窗口的 bounding 形状设为矩形并集（窗口内坐标，
 * rects 是 n 组 {x,y,w,h}）。形状外的像素不显示、也不接收事件。 */
int x11_shape_probe(x11_t* c);
int x11_shape_rects(x11_t* c, uint32_t win, const int16_t* rects, int n);

/* 属性 */
uint32_t x11_intern_atom(x11_t* c, const char* name);
int x11_set_property32(x11_t* c, uint32_t win, uint32_t prop, uint32_t type,
                       const uint32_t* data, int count);
int x11_delete_property(x11_t* c, uint32_t win, uint32_t prop);

/* 事件：返回 1=拿到一个事件（写入 ev[32]），0=暂时没有，-1=连接断开 */
int x11_next_event(x11_t* c, uint8_t* ev);

/* 常用取值 */
static inline uint32_t x11_ev_u32(const uint8_t* ev, int off) {
  return (uint32_t)ev[off] | ((uint32_t)ev[off + 1] << 8) |
         ((uint32_t)ev[off + 2] << 16) | ((uint32_t)ev[off + 3] << 24);
}

#endif /* YIYIYA_APP_X11_H */
