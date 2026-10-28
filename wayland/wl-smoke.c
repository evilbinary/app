/* wayland 端到端冒烟：server（libwayland-server + wl_shm + wl_compositor）
 * 与 client（libwayland-client）在同一进程里 fork 成两个进程，跑通：
 *
 *   unix socket 握手 → wl_registry 枚举 → wl_shm.create_pool
 *   （memfd fd 经 SCM_RIGHTS 传过去）→ create_buffer → surface
 *   attach + commit → server 读共享页内容校验像素 → buffer.release
 *
 * 这覆盖了 wayland 在本内核上最关键的三个地基：unix socket、fd 传递、
 * 共享页一致性。不画屏：像素校验通过即算链路打通（fbdev 输出是下一步）。
 *
 * 输出 LTP 风格 TPASS/TFAIL + Summary，tools/ltp_batch.py 直跑。 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <wayland-server.h>
#include <wayland-server-protocol.h>

#define W 64
#define H 64
#define PIXEL_RED 0xFFFF0000u

/* ===================== server 侧 ===================== */

struct surface_state {
  struct wl_resource* pending_buffer;
};

static int g_server_ok;
static int g_server_done;
static int g_result_pipe = -1;
static struct wl_display* g_display;

static void surf_destroy(struct wl_client* c, struct wl_resource* r) {
  (void)c;
  wl_resource_destroy(r);
}

static void surf_attach(struct wl_client* c, struct wl_resource* r,
                        struct wl_resource* buffer, int32_t x, int32_t y) {
  struct surface_state* s = wl_resource_get_user_data(r);
  (void)c;
  (void)x;
  (void)y;
  printf("SERVER: attach buffer=%p\n", (void*)buffer);
  fflush(stdout);
  s->pending_buffer = buffer;
}

static void surf_damage(struct wl_client* c, struct wl_resource* r, int32_t x,
                        int32_t y, int32_t w, int32_t h) {
  (void)c; (void)r; (void)x; (void)y; (void)w; (void)h;
}

static void surf_frame(struct wl_client* c, struct wl_resource* r,
                       uint32_t callback) {
  struct wl_resource* cb;
  (void)r;
  cb = wl_resource_create(c, &wl_callback_interface, 1, callback);
  if (cb == NULL) {
    return;
  }
  wl_callback_send_done(cb, 0);
  wl_resource_destroy(cb);
}

static void surf_set_region(struct wl_client* c, struct wl_resource* r,
                            struct wl_resource* region) {
  (void)c; (void)r; (void)region;
}

static void surf_commit(struct wl_client* c, struct wl_resource* r) {
  struct surface_state* s = wl_resource_get_user_data(r);
  if (s->pending_buffer != NULL) {
    struct wl_shm_buffer* b = wl_shm_buffer_get(s->pending_buffer);
    if (b != NULL) {
      uint32_t* px;
      int w;
      int h;
      int stride;
      wl_shm_buffer_begin_access(b);
      px = (uint32_t*)wl_shm_buffer_get_data(b);
      w = wl_shm_buffer_get_width(b);
      h = wl_shm_buffer_get_height(b);
      stride = wl_shm_buffer_get_stride(b);
      printf("SERVER: buffer %dx%d stride=%d first=0x%08x last=0x%08x\n", w, h,
             stride, px != NULL ? px[0] : 0,
             (px != NULL && w > 0) ? px[w - 1] : 0);
      if (px != NULL && px[0] == PIXEL_RED) {
        printf("TPASS: server 读到 client 画的像素（共享页一致）\n");
        g_server_ok = 1;
      } else {
        printf("TFAIL: server 像素不符\n");
      }
      wl_shm_buffer_end_access(b);
    } else {
      printf("TFAIL: attach 的不是 shm buffer\n");
    }
    wl_buffer_send_release(s->pending_buffer);
    s->pending_buffer = NULL;
  }
  wl_client_flush(c);
  if (!g_server_done) {
    char r = g_server_ok ? 1 : 0;
    g_server_done = 1;
    if (g_result_pipe >= 0) {
      ssize_t ignored = write(g_result_pipe, &r, 1);
      (void)ignored;
    }
    wl_display_terminate(g_display);
  }
}

static void surf_set_buffer_transform(struct wl_client* c, struct wl_resource* r,
                                      int32_t transform) {
  (void)c; (void)r;
  if (transform != 0) {
    printf("SERVER: unsupported transform %d\n", transform);
  }
}

static void surf_set_buffer_scale(struct wl_client* c, struct wl_resource* r,
                                  int32_t scale) {
  (void)c; (void)r; (void)scale;
}

static void surf_damage_buffer(struct wl_client* c, struct wl_resource* r,
                               int32_t x, int32_t y, int32_t w, int32_t h) {
  (void)c; (void)r; (void)x; (void)y; (void)w; (void)h;
}

static void surf_offset(struct wl_client* c, struct wl_resource* r, int32_t x,
                        int32_t y) {
  (void)c; (void)r; (void)x; (void)y;
}

static const struct wl_surface_interface surface_impl = {
    .destroy = surf_destroy,
    .attach = surf_attach,
    .damage = surf_damage,
    .frame = surf_frame,
    .set_opaque_region = surf_set_region,
    .set_input_region = surf_set_region,
    .commit = surf_commit,
    .set_buffer_transform = surf_set_buffer_transform,
    .set_buffer_scale = surf_set_buffer_scale,
    .damage_buffer = surf_damage_buffer,
    .offset = surf_offset,
};

static void surface_resource_destroy(struct wl_resource* r) {
  struct surface_state* s = wl_resource_get_user_data(r);
  if (s != NULL) {
    free(s);
  }
}

static void compositor_create_surface(struct wl_client* c, struct wl_resource* r,
                                      uint32_t id) {
  struct surface_state* s = calloc(1, sizeof(*s));
  struct wl_resource* res;
  printf("SERVER: create_surface id=%u\n", id);
  fflush(stdout);
  if (s == NULL) {
    wl_client_post_no_memory(c);
    return;
  }
  res = wl_resource_create(c, &wl_surface_interface, wl_resource_get_version(r),
                           id);
  if (res == NULL) {
    free(s);
    wl_client_post_no_memory(c);
    return;
  }
  wl_resource_set_implementation(res, &surface_impl, s,
                                 surface_resource_destroy);
}

static void compositor_create_region(struct wl_client* c, struct wl_resource* r,
                                     uint32_t id) {
  /* 不用 region：给个空壳对象，避免客户端 destroy 时报错 */
  struct wl_resource* res = wl_resource_create(
      c, &wl_region_interface, wl_resource_get_version(r), id);
  (void)res;
}

static const struct wl_compositor_interface compositor_impl = {
    .create_surface = compositor_create_surface,
    .create_region = compositor_create_region,
};

static void compositor_bind(struct wl_client* client, void* data,
                            uint32_t version, uint32_t id) {
  struct wl_resource* r = wl_resource_create(
      client, &wl_compositor_interface, version, id);
  (void)data;
  if (r == NULL) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(r, &compositor_impl, NULL, NULL);
}

static int server_run(int wfd) {
  const char* sock;

  g_result_pipe = wfd;
  g_display = wl_display_create();
  if (g_display == NULL) {
    printf("SERVER: wl_display_create failed\n");
    return 1;
  }
  /* wl_shm：内部处理 create_pool/create_buffer，收到的是 SCM_RIGHTS 传的 fd */
  wl_display_init_shm(g_display);
  if (wl_global_create(g_display, &wl_compositor_interface, 1, NULL,
                       compositor_bind) == NULL) {
    printf("SERVER: wl_global_create compositor failed\n");
    return 1;
  }
  sock = wl_display_add_socket_auto(g_display);
  if (sock == NULL) {
    printf("SERVER: add_socket failed（XDG_RUNTIME_DIR=%s）\n",
           getenv("XDG_RUNTIME_DIR") != NULL ? getenv("XDG_RUNTIME_DIR")
                                             : "(unset)");
    return 1;
  }
  printf("TPASS: compositor listening on %s\n", sock);
  fflush(stdout);
  wl_display_run(g_display); /* commit 里 terminate */
  wl_display_destroy(g_display);
  return g_server_ok ? 0 : 1;
}

/* ===================== client 侧 ===================== */

static struct wl_compositor* c_compositor;
static struct wl_shm* c_shm;

static void reg_global(void* data, struct wl_registry* reg, uint32_t name,
                       const char* iface, uint32_t version) {
  (void)data;
  if (strcmp(iface, wl_compositor_interface.name) == 0) {
    c_compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 1);
    printf("TPASS: client bind wl_compositor v%u\n", version);
  } else if (strcmp(iface, wl_shm_interface.name) == 0) {
    c_shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
    printf("TPASS: client bind wl_shm v%u\n", version);
  }
}

static void reg_global_remove(void* data, struct wl_registry* reg,
                              uint32_t name) {
  (void)data;
  (void)reg;
  (void)name;
}

static const struct wl_registry_listener reg_listener = {
    .global = reg_global,
    .global_remove = reg_global_remove,
};

static void buffer_release(void* data, struct wl_buffer* buffer) {
  (void)data;
  (void)buffer;
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release,
};

static int client_run(void) {
  struct wl_display* display;
  struct wl_registry* registry;
  struct wl_compositor* compositor;
  struct wl_shm* shm;
  struct wl_shm_pool* pool;
  struct wl_buffer* buffer;
  struct wl_surface* surface;
  uint32_t* pixels;
  int stride = W * 4;
  int size = stride * H;
  int fd;
  int i;
  int ret = 1;

  display = wl_display_connect(NULL);
  if (display == NULL) {
    printf("TFAIL: wl_display_connect failed（WAYLAND_DISPLAY=%s）\n",
           getenv("WAYLAND_DISPLAY") != NULL ? getenv("WAYLAND_DISPLAY")
                                              : "(unset)");
    return 1;
  }
  printf("TPASS: client connected\n");

  registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &reg_listener, NULL);
  {
    int r1 = wl_display_roundtrip(display);
    int r2 = wl_display_roundtrip(display);
    printf("TINFO: roundtrip1=%d roundtrip2=%d wl_err=%d errno=%d\n", r1, r2,
           wl_display_get_error(display), errno);
  }
  compositor = c_compositor;
  shm = c_shm;
  if (compositor == NULL || shm == NULL) {
    printf("TFAIL: compositor=%p shm=%p\n", (void*)compositor, (void*)shm);
    goto out_display;
  }

  /* wl_shm 池：memfd + ftruncate + MAP_SHARED（本内核的共享页对象） */
  fd = memfd_create("wl-smoke-shm", 0);
  if (fd < 0) {
    printf("TFAIL: memfd_create errno=%d\n", errno);
    goto out_display;
  }
  if (ftruncate(fd, size) != 0) {
    printf("TFAIL: ftruncate errno=%d\n", errno);
    goto out_display;
  }
  pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (pixels == MAP_FAILED) {
    printf("TFAIL: mmap errno=%d\n", errno);
    goto out_display;
  }
  for (i = 0; i < W * H; i++) {
    pixels[i] = PIXEL_RED;
  }
  printf("TPASS: client 填好 %dx%d 像素\n", W, H);

  pool = wl_shm_create_pool(shm, fd, size); /* ← fd 走 SCM_RIGHTS */
  buffer = wl_shm_pool_create_buffer(pool, 0, W, H, stride,
                                     WL_SHM_FORMAT_XRGB8888);
  wl_buffer_add_listener(buffer, &buffer_listener, NULL);
  surface = wl_compositor_create_surface(compositor);
  wl_surface_attach(surface, buffer, 0, 0);
  wl_surface_commit(surface);
  printf("TPASS: client attach+commit\n");
  {
    int fr = wl_display_flush(display);
    printf("TINFO: flush=%d errno=%d\n", fr, fr < 0 ? errno : 0);
  }
  /* 等 server 处理完 commit（server 会在 commit 里 terminate 并写结果） */
  {
    int rr = wl_display_roundtrip(display);
    printf("TINFO: roundtrip=%d errno=%d wl_err=%d\n", rr,
           rr < 0 ? errno : 0, wl_display_get_error(display));
  }
  ret = 0;

out_display:
  wl_display_disconnect(display);
  return ret;
}

int main(int argc, char* argv[]) {
  int fds[2];
  pid_t pid;
  int st = 0;
  char result = 0;
  int client_ret;
  ssize_t n;

  (void)argc;
  (void)argv;
  /* 本内核没有 /run/user/1000；wayland 默认 socket 目录就是它 */
  setenv("XDG_RUNTIME_DIR", "/tmp", 1);
  setenv("WAYLAND_DISPLAY", "wayland-0", 1);

  if (pipe(fds) != 0) {
    printf("TFAIL: pipe errno=%d\n", errno);
    return 1;
  }
  pid = fork();
  if (pid < 0) {
    printf("TFAIL: fork errno=%d\n", errno);
    return 1;
  }
  if (pid == 0) {
    /* 子进程：server。父进程不读 pipe 前不要阻塞在 run 之外 */
    close(fds[0]);
    _exit(server_run(fds[1]));
  }

  close(fds[1]);
  /* 等 server 建好 socket：connect 失败就重试几次（最多 ~2s） */
  {
    int tries;
    for (tries = 0; tries < 20; tries++) {
      struct wl_display* probe = wl_display_connect(NULL);
      if (probe != NULL) {
        wl_display_disconnect(probe);
        break;
      }
      usleep(100 * 1000);
    }
  }
  client_ret = client_run();
  n = read(fds[0], &result, 1);
  close(fds[0]);
  waitpid(pid, &st, 0);

  printf("TINFO: client_ret=%d result=%d server_exit=%d\n", client_ret,
         (int)result, WIFEXITED(st) ? WEXITSTATUS(st) : -1);
  if (n == 1 && result == 1 && client_ret == 0) {
    printf("TPASS: wayland 端到端握手 + fd 传递 + 共享页全部通过\n");
  } else {
    printf("TFAIL: 端到端链路未通过\n");
  }
  printf("\nSummary:\n");
  return (n == 1 && result == 1 && client_ret == 0) ? 0 : 1;
}


