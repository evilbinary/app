/* Wayland + EGL(device)/GLES2 演示框架。
 *
 * 渲染路径：
 *   EGL device 平台（gpud/V3D）→ surfaceless ES2 context → FBO
 *   → glReadPixels → wl_shm buffer → wl_surface.commit
 *
 * 不用 wayland-egl / EGL_PLATFORM_WAYLAND：本平台的 EGL 只有 device
 * 平台（"EGL Wayland extension: no"），所以 GL 结果统一走 shm 上屏。
 */
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <cairo.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "shared/cairo-util.h"

#include "demo.h"

#ifndef EGL_PLATFORM_DEVICE_EXT
#define EGL_PLATFORM_DEVICE_EXT 0x313F
#endif
#ifndef SYS_memfd_create
#define SYS_memfd_create 279
#endif

#define DEMO_W 640
#define DEMO_H 480
#define FRAME_MS 16

struct demo_state {
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct xdg_wm_base *wm_base;
  struct wl_surface *surface;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *toplevel;
  struct wl_buffer *buffer[2];
  void *shm_data[2];
  int busy[2];
  int buf_index;
  int width, height, stride;
  int configured, running;
  struct wl_seat *seat;
  struct wl_pointer *pointer;
  int ptr_x, ptr_y;

  /* eventdemo 同款客户端装饰：theme/frame 来自 eggs/weston/shared。 */
  struct theme *theme;
  struct frame *frame;
  cairo_surface_t *frame_surf;
  int win_w, win_h;
  int content_x, content_y;
  int maximized;

  int (*init)(void);
  void (*draw)(float t);
  double start;
};

static const char *g_title = "mesa demo";
void demo_set_title(const char *title) { g_title = title; }

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static EGLDisplay g_dpy = EGL_NO_DISPLAY;
static EGLContext g_ctx = EGL_NO_CONTEXT;
static EGLConfig g_cfg;
static GLuint g_fbo, g_tex, g_depth;
static uint8_t *g_readback;

/* --------------------------------------------------------------- matrix */

void mat4_identity(mat4 *m) {
  memset(m, 0, sizeof(*m));
  m->m[0] = m->m[5] = m->m[10] = m->m[15] = 1.0f;
}

void mat4_mul(mat4 *out, const mat4 *a, const mat4 *b) {
  mat4 r;
  int i, j, k;
  for (j = 0; j < 4; j++) {
    for (i = 0; i < 4; i++) {
      float s = 0;
      for (k = 0; k < 4; k++) {
        s += a->m[k * 4 + i] * b->m[j * 4 + k];
      }
      r.m[j * 4 + i] = s;
    }
  }
  *out = r;
}

void mat4_translate(mat4 *m, float x, float y, float z) {
  mat4 t;
  mat4_identity(&t);
  t.m[12] = x;
  t.m[13] = y;
  t.m[14] = z;
  mat4_mul(m, m, &t);
}

void mat4_scale(mat4 *m, float x, float y, float z) {
  mat4 t;
  mat4_identity(&t);
  t.m[0] = x;
  t.m[5] = y;
  t.m[10] = z;
  mat4_mul(m, m, &t);
}

void mat4_rotate(mat4 *m, float angle_deg, float ax, float ay, float az) {
  float a = angle_deg * (float)M_PI / 180.0f;
  float c = cosf(a), s = sinf(a);
  float len = sqrtf(ax * ax + ay * ay + az * az);
  float x, y, z, t;
  mat4 r;
  if (len < 1e-6f) {
    return;
  }
  x = ax / len;
  y = ay / len;
  z = az / len;
  memset(&r, 0, sizeof(r));
  t = 1.0f - c;
  r.m[0] = t * x * x + c;
  r.m[1] = t * x * y + s * z;
  r.m[2] = t * x * z - s * y;
  r.m[4] = t * x * y - s * z;
  r.m[5] = t * y * y + c;
  r.m[6] = t * y * z + s * x;
  r.m[8] = t * x * z + s * y;
  r.m[9] = t * y * z - s * x;
  r.m[10] = t * z * z + c;
  r.m[15] = 1.0f;
  mat4_mul(m, m, &r);
}

void mat4_perspective(mat4 *m, float fovy_deg, float aspect, float znear,
                      float zfar) {
  float f = 1.0f / tanf(fovy_deg * (float)M_PI / 360.0f);
  memset(m, 0, sizeof(*m));
  m->m[0] = f / aspect;
  m->m[5] = f;
  m->m[10] = (zfar + znear) / (znear - zfar);
  m->m[11] = -1.0f;
  m->m[14] = (2.0f * zfar * znear) / (znear - zfar);
}

/* --------------------------------------------------------------- shader */

static GLuint compile(GLenum type, const char *src) {
  GLuint s = glCreateShader(type);
  GLint ok = 0;
  glShaderSource(s, 1, &src, NULL);
  glCompileShader(s);
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024];
    glGetShaderInfoLog(s, sizeof(log), NULL, log);
    fprintf(stderr, "shader compile failed: %s\n", log);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

GLuint demo_program(const char *vs, const char *fs) {
  GLuint v = compile(GL_VERTEX_SHADER, vs);
  GLuint f = compile(GL_FRAGMENT_SHADER, fs);
  GLuint p;
  GLint ok = 0;
  if (!v || !f) {
    if (v) glDeleteShader(v);
    if (f) glDeleteShader(f);
    return 0;
  }
  p = glCreateProgram();
  glAttachShader(p, v);
  glAttachShader(p, f);
  glLinkProgram(p);
  glDeleteShader(v);
  glDeleteShader(f);
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[1024];
    glGetProgramInfoLog(p, sizeof(log), NULL, log);
    fprintf(stderr, "program link failed: %s\n", log);
    glDeleteProgram(p);
    return 0;
  }
  return p;
}

/* --------------------------------------------------------------- sphere */

void demo_sphere(float radius, int slices, int stacks, float **verts,
                 int *vert_count) {
  int i, j, n = 0;
  float *v = malloc((size_t)slices * stacks * 6 * 6 * sizeof(float));
  if (!v) {
    *verts = NULL;
    *vert_count = 0;
    return;
  }
  for (j = 0; j < stacks; j++) {
    float t0 = (float)j / stacks * (float)M_PI;
    float t1 = (float)(j + 1) / stacks * (float)M_PI;
    for (i = 0; i < slices; i++) {
      float p0 = (float)i / slices * 2.0f * (float)M_PI;
      float p1 = (float)(i + 1) / slices * 2.0f * (float)M_PI;
      float pts[4][3] = {
          {sinf(t0) * cosf(p0), cosf(t0), sinf(t0) * sinf(p0)},
          {sinf(t0) * cosf(p1), cosf(t0), sinf(t0) * sinf(p1)},
          {sinf(t1) * cosf(p1), cosf(t1), sinf(t1) * sinf(p1)},
          {sinf(t1) * cosf(p0), cosf(t1), sinf(t1) * sinf(p0)},
      };
      int tri[6] = {0, 1, 2, 0, 2, 3};
      int k;
      for (k = 0; k < 6; k++) {
        int p = tri[k];
        v[n++] = pts[p][0] * radius;
        v[n++] = pts[p][1] * radius;
        v[n++] = pts[p][2] * radius;
        v[n++] = pts[p][0];
        v[n++] = pts[p][1];
        v[n++] = pts[p][2];
      }
    }
  }
  *verts = v;
  *vert_count = n / 6;
}

/* --------------------------------------------------------------- EGL */

static int egl_setup(int w, int h) {
  PFNEGLQUERYDEVICESEXTPROC query_devices =
      (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
  PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display =
      (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress(
          "eglGetPlatformDisplayEXT");
  EGLDeviceEXT devs[8];
  EGLint ndev = 0, major = 0, minor = 0, ncfg = 0;
  EGLint cfg_attribs[] = {
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE,
  };
  EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

  if (!query_devices || !get_platform_display) {
    fprintf(stderr, "EGL device entrypoints missing\n");
    return -1;
  }
  if (!query_devices(8, devs, &ndev) || ndev < 1) {
    fprintf(stderr, "eglQueryDevicesEXT failed\n");
    return -1;
  }
  g_dpy = get_platform_display(EGL_PLATFORM_DEVICE_EXT, devs[0], NULL);
  if (g_dpy == EGL_NO_DISPLAY || !eglInitialize(g_dpy, &major, &minor)) {
    fprintf(stderr, "eglInitialize failed (0x%x)\n", eglGetError());
    return -1;
  }
  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    fprintf(stderr, "eglBindAPI ES failed\n");
    return -1;
  }
  if (!eglChooseConfig(g_dpy, cfg_attribs, &g_cfg, 1, &ncfg) || ncfg < 1) {
    fprintf(stderr, "eglChooseConfig failed\n");
    return -1;
  }
  g_ctx = eglCreateContext(g_dpy, g_cfg, EGL_NO_CONTEXT, ctx_attribs);
  if (g_ctx == EGL_NO_CONTEXT ||
      !eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, g_ctx)) {
    fprintf(stderr, "eglMakeCurrent failed (0x%x)\n", eglGetError());
    return -1;
  }
  printf("demo: GL_VERSION=%s\n", glGetString(GL_VERSION));
  printf("demo: GL_RENDERER=%s\n", glGetString(GL_RENDERER));

  glGenTextures(1, &g_tex);
  glBindTexture(GL_TEXTURE_2D, g_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glGenRenderbuffers(1, &g_depth);
  glBindRenderbuffer(GL_RENDERBUFFER, g_depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);
  glGenFramebuffers(1, &g_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         g_tex, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, g_depth);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "FBO incomplete\n");
    return -1;
  }
  glViewport(0, 0, w, h);
  glEnable(GL_DEPTH_TEST);
  g_readback = malloc((size_t)w * h * 4);
  return g_readback ? 0 : -1;
}

/* --------------------------------------------------------------- CSD */
/* eventdemo 同款装饰：Weston 的 shared/frame.c 用 cairo 画标题栏、
 * 关闭/最大化/最小化按钮和阴影，输入命中测试也复用它。 */
static void frame_repaint_surface(struct demo_state *d) {
  cairo_t *cr;
  if (!d->frame || !d->frame_surf) return;
  cr = cairo_create(d->frame_surf);
  frame_repaint(d->frame, cr);
  cairo_destroy(cr);
  cairo_surface_flush(d->frame_surf);
}

static void frame_after_input(struct demo_state *d, uint32_t serial,
                             enum theme_location loc) {
  uint32_t st = frame_status(d->frame);

  if (st & FRAME_STATUS_CLOSE) {
    d->running = 0;
  }
  if (st & FRAME_STATUS_MAXIMIZE) {
    d->maximized = !d->maximized;
    if (d->maximized) {
      xdg_toplevel_set_maximized(d->toplevel);
      frame_set_flag(d->frame, FRAME_FLAG_MAXIMIZED);
    } else {
      xdg_toplevel_unset_maximized(d->toplevel);
      frame_unset_flag(d->frame, FRAME_FLAG_MAXIMIZED);
    }
  }
  if (st & FRAME_STATUS_MINIMIZE) {
    xdg_toplevel_set_minimized(d->toplevel);
  }
  if ((st & FRAME_STATUS_MOVE) && d->seat) {
    xdg_toplevel_move(d->toplevel, d->seat, serial);
  }
  if ((st & FRAME_STATUS_RESIZE) && d->seat) {
    xdg_toplevel_resize(d->toplevel, d->seat, serial, loc);
  }
  frame_status_clear(d->frame, st);
  if (st & FRAME_STATUS_REPAINT) {
    frame_repaint_surface(d);
  }
}

static void ptr_enter(void *data, struct wl_pointer *p, uint32_t serial,
                      struct wl_surface *s, wl_fixed_t x, wl_fixed_t y) {
  struct demo_state *d = data;
  (void)p; (void)serial; (void)s;
  d->ptr_x = wl_fixed_to_int(x);
  d->ptr_y = wl_fixed_to_int(y);
  frame_pointer_enter(d->frame, d, d->ptr_x, d->ptr_y);
  frame_after_input(d, 0, THEME_LOCATION_INTERIOR);
}
static void ptr_leave(void *data, struct wl_pointer *p, uint32_t serial,
                      struct wl_surface *s) {
  struct demo_state *d = data;
  (void)p; (void)serial; (void)s;
  frame_pointer_leave(d->frame, d);
  frame_after_input(d, 0, THEME_LOCATION_INTERIOR);
}
static void ptr_motion(void *data, struct wl_pointer *p, uint32_t time,
                       wl_fixed_t x, wl_fixed_t y) {
  struct demo_state *d = data;
  (void)p; (void)time;
  d->ptr_x = wl_fixed_to_int(x);
  d->ptr_y = wl_fixed_to_int(y);
  frame_pointer_motion(d->frame, d, d->ptr_x, d->ptr_y);
  frame_after_input(d, 0, THEME_LOCATION_INTERIOR);
}
static void ptr_button(void *data, struct wl_pointer *p, uint32_t serial,
                       uint32_t time, uint32_t button, uint32_t state) {
  struct demo_state *d = data;
  enum wl_pointer_button_state bs =
      state ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED;
  (void)p; (void)time;
  {
    enum theme_location loc = frame_pointer_button(d->frame, d, button, bs);
    frame_after_input(d, serial, loc);
  }
}
static void ptr_axis(void *data, struct wl_pointer *p, uint32_t time,
                     uint32_t axis, wl_fixed_t value) {
  (void)data; (void)p; (void)time; (void)axis; (void)value;
}
static const struct wl_pointer_listener pointer_listener = {
    ptr_enter, ptr_leave, ptr_motion, ptr_button, ptr_axis};

static void seat_caps(void *data, struct wl_seat *seat, uint32_t caps) {
  struct demo_state *d = data;
  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !d->pointer) {
    d->pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(d->pointer, &pointer_listener, d);
  }
}
static const struct wl_seat_listener seat_listener = {seat_caps};

/* --------------------------------------------------------------- wayland */

static void registry_global(void *data, struct wl_registry *reg, uint32_t id,
                            const char *iface, uint32_t version) {
  struct demo_state *d = data;
  if (strcmp(iface, "wl_compositor") == 0) {
    d->compositor = wl_registry_bind(reg, id, &wl_compositor_interface, 1);
  } else if (strcmp(iface, "wl_shm") == 0) {
    d->shm = wl_registry_bind(reg, id, &wl_shm_interface, 1);
  } else if (strcmp(iface, "xdg_wm_base") == 0) {
    d->wm_base = wl_registry_bind(reg, id, &xdg_wm_base_interface, 1);
  } else if (strcmp(iface, "wl_seat") == 0) {
    d->seat = wl_registry_bind(reg, id, &wl_seat_interface, 1);
    wl_seat_add_listener(d->seat, &seat_listener, d);
  }
}
static void registry_remove(void *data, struct wl_registry *reg, uint32_t id) {}
static const struct wl_registry_listener registry_listener = {
    registry_global, registry_remove};

static void wm_ping(void *data, struct xdg_wm_base *wm, uint32_t serial) {
  xdg_wm_base_pong(wm, serial);
}
static const struct xdg_wm_base_listener wm_listener = {wm_ping};

static void xdg_surface_configure(void *data, struct xdg_surface *s,
                                  uint32_t serial) {
  struct demo_state *d = data;
  xdg_surface_ack_configure(s, serial);
  if (!d->configured) {
    d->configured = 1;
  }
}
static const struct xdg_surface_listener xdg_surface_listener = {
    xdg_surface_configure};

static void toplevel_configure(void *data, struct xdg_toplevel *t, int32_t w,
                               int32_t h, struct wl_array *states) {}
static void toplevel_close(void *data, struct xdg_toplevel *t) {
  struct demo_state *d = data;
  d->running = 0;
}
static const struct xdg_toplevel_listener toplevel_listener = {
    toplevel_configure, toplevel_close};

static void buf_release(void *data, struct wl_buffer *b) {
  (void)b;
  *(int *)data = 0;
}
static const struct wl_buffer_listener buf_listener = {buf_release};

static int shm_buffer_create(struct demo_state *d, int idx) {
  size_t size = (size_t)d->stride * (size_t)d->win_h;
  struct wl_shm_pool *pool;
  int fd = (int)syscall(SYS_memfd_create, "wl-demo", 1);
  if (fd < 0) {
    perror("memfd_create");
    return -1;
  }
  if (ftruncate(fd, (off_t)size) < 0) {
    perror("ftruncate");
    close(fd);
    return -1;
  }
  d->shm_data[idx] = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                          0);
  if (d->shm_data[idx] == MAP_FAILED) {
    perror("mmap shm");
    close(fd);
    return -1;
  }
  pool = wl_shm_create_pool(d->shm, fd, (int)size);
  d->buffer[idx] = wl_shm_pool_create_buffer(pool, 0, d->win_w, d->win_h,
                                             d->stride,
                                             WL_SHM_FORMAT_ARGB8888);
  wl_shm_pool_destroy(pool);
  close(fd);
  if (d->buffer[idx]) {
    d->busy[idx] = 0;
    wl_buffer_add_listener(d->buffer[idx], &buf_listener, &d->busy[idx]);
  }
  return d->buffer[idx] ? 0 : -1;
}

static double now_sec(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void present(struct demo_state *d) {
  uint32_t *dst = (uint32_t *)d->shm_data[d->buf_index];
  uint8_t *src = g_readback;
  int x, y;

  glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
  glViewport(0, 0, d->width, d->height);
  glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (d->draw) {
    d->draw((float)(now_sec() - d->start));
  }
  glFinish();
  glReadPixels(0, 0, d->width, d->height, GL_RGBA, GL_UNSIGNED_BYTE,
               g_readback);

  /* 【双缓冲】合成器还在读的 buffer 不能覆盖。 */
  {
    int pick = -1;
    if (!d->busy[d->buf_index]) {
      pick = d->buf_index;
    } else if (!d->busy[d->buf_index ^ 1]) {
      pick = d->buf_index ^ 1;
    }
    if (pick < 0) {
      return;
    }
    d->busy[pick] = 1;
    dst = (uint32_t *)d->shm_data[pick];

  /* 先把 cairo 画的 eventdemo 同款 frame（阴影/标题栏/按钮）整块拷进
   * shm；cairo ARGB32 与 WL_SHM_FORMAT_ARGB8888 的字节序一致。 */
  {
    const uint32_t *fs = (const uint32_t *)cairo_image_surface_get_data(
        d->frame_surf);
    int fstride = cairo_image_surface_get_stride(d->frame_surf) / 4;
    for (y = 0; y < d->win_h; y++) {
      memcpy(dst + (size_t)y * d->win_w, fs + (size_t)y * fstride,
             (size_t)d->win_w * 4);
    }
  }
  /* GL 内容（原点左下）覆盖到 frame 的 interior 区域。 */
  for (y = 0; y < d->height; y++) {
    const uint8_t *s = src + (size_t)(d->height - 1 - y) * d->width * 4;
    uint32_t *row = dst + (size_t)(d->content_y + y) * d->win_w +
                    d->content_x;
    for (x = 0; x < d->width; x++) {
      row[x] = 0xff000000u | ((uint32_t)s[x * 4 + 2] << 16) |
               ((uint32_t)s[x * 4 + 1] << 8) | (uint32_t)s[x * 4 + 0];
    }
  }

  wl_surface_attach(d->surface, d->buffer[pick], 0, 0);
  wl_surface_damage(d->surface, 0, 0, d->win_w, d->win_h);
  wl_surface_commit(d->surface);
  wl_display_flush(d->display);
  d->buf_index = pick ^ 1;
  }
}

static int timer_cb(void *data) {
  struct demo_state *d = data;
  if (d->configured && d->running) {
    present(d);
  }
  return 1;
}

int demo_run(int (*init)(void), void (*draw)(float t)) {
  struct demo_state d;
  struct wl_event_loop *loop;
  struct wl_event_source *timer;

  memset(&d, 0, sizeof(d));
  d.width = DEMO_W;
  d.height = DEMO_H;
  d.stride = DEMO_W * 4;
  d.init = init;
  d.draw = draw;
  d.running = 1;
  d.start = now_sec();

  if (egl_setup(d.width, d.height) < 0) {
    return 1;
  }
  if (init && init() != 0) {
    fprintf(stderr, "demo init failed\n");
    return 1;
  }

  d.display = wl_display_connect(NULL);
  if (!d.display) {
    /* weston 的 socket 固定在 /lib/wayland-0；老 shell 可能给 /tmp。 */
    setenv("XDG_RUNTIME_DIR", "/lib", 1);
    d.display = wl_display_connect(NULL);
  }
  if (!d.display) {
    fprintf(stderr, "wl_display_connect failed (XDG_RUNTIME_DIR=%s)\n",
            getenv("XDG_RUNTIME_DIR") ? getenv("XDG_RUNTIME_DIR") : "?");
    return 1;
  }
  d.registry = wl_display_get_registry(d.display);
  wl_registry_add_listener(d.registry, &registry_listener, &d);
  wl_display_roundtrip(d.display);
  if (!d.compositor || !d.shm || !d.wm_base) {
    fprintf(stderr, "missing globals: compositor=%p shm=%p wm_base=%p\n",
            (void *)d.compositor, (void *)d.shm, (void *)d.wm_base);
    return 1;
  }
  xdg_wm_base_add_listener(d.wm_base, &wm_listener, &d);

  /* eventdemo 同款 CSD：theme/frame 来自 eggs/weston/shared/frame.c。 */
  d.theme = theme_create();
  d.frame = frame_create(d.theme, DEMO_W, DEMO_H, FRAME_BUTTON_ALL, g_title,
                         NULL);
  if (d.frame) {
    int iw = DEMO_W, ih = DEMO_H;
    /* frame_create 的宽高是整个窗口；这里把 interior 调成内容尺寸，
     * 窗口总尺寸自动加上标题栏/边框/阴影。 */
    frame_resize_inside(d.frame, DEMO_W, DEMO_H);
    d.win_w = frame_width(d.frame);
    d.win_h = frame_height(d.frame);
    frame_interior(d.frame, &d.content_x, &d.content_y, &iw, &ih);
    d.width = iw;
    d.height = ih;
    d.stride = (uint32_t)d.win_w * 4;
    d.frame_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, d.win_w,
                                              d.win_h);
    frame_repaint_surface(&d);
    printf("demo: frame %dx%d content %dx%d at %d,%d\n", d.win_w, d.win_h,
           d.width, d.height, d.content_x, d.content_y);
  } else {
    d.win_w = DEMO_W;
    d.win_h = DEMO_H;
  }

  d.surface = wl_compositor_create_surface(d.compositor);
  d.xdg_surface = xdg_wm_base_get_xdg_surface(d.wm_base, d.surface);
  xdg_surface_add_listener(d.xdg_surface, &xdg_surface_listener, &d);
  d.toplevel = xdg_surface_get_toplevel(d.xdg_surface);
  xdg_toplevel_add_listener(d.toplevel, &toplevel_listener, &d);
  xdg_toplevel_set_title(d.toplevel, g_title);
  xdg_toplevel_set_app_id(d.toplevel, "yiyiya.mesa-demo");
  wl_surface_commit(d.surface);

  if (shm_buffer_create(&d, 0) < 0 || shm_buffer_create(&d, 1) < 0) {
    fprintf(stderr, "shm buffer create failed\n");
    return 1;
  }

  loop = wl_display_get_event_loop(d.display);
  timer = wl_event_loop_add_timer(loop, timer_cb, &d);
  wl_event_source_timer_update(timer, FRAME_MS);

  while (d.running) {
    if (wl_display_dispatch(d.display) < 0) {
      break;
    }
  }

  wl_event_source_remove(timer);
  xdg_toplevel_destroy(d.toplevel);
  xdg_surface_destroy(d.xdg_surface);
  wl_surface_destroy(d.surface);
  wl_display_disconnect(d.display);
  eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(g_dpy, g_ctx);
  eglTerminate(g_dpy);
  return 0;
}
