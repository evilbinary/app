/* weston 端到端冒烟（阶段 2）：在本内核上把 libweston 拉起来。
 *
 * 结构：一个二进制 fork 出 server / client 两个进程。
 *   server：wl_display_create → weston_compositor_create（pixman 渲染器）
 *           → headless 后端（weston_backend_init）→ 创建并 enable 一个
 *           640x480 输出 → 监听 unix socket → wl_display_run
 *   client：连 socket，枚举 registry，检查核心 global 是否齐（wl_compositor /
 *           wl_subcompositor / wl_shm / wl_output / wp_viewporter /
 *           wp_presentation / zxdg_output_manager_v1），roundtrip 成功即算通。
 *
 * 这一步验证的是 weston 的事件循环（epoll + timerfd）、pixman 渲染器初始化、
 * headless 输出 enable、协议 global 注册。像素上屏（fbdev/DRM 后端）不在本阶段。
 *
 * 输出 LTP 风格 TPASS/TFAIL + Summary，tools/ltp_batch.py 直跑。
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland-server.h>

#include <libweston/backend-headless.h>
#include <libweston/libweston.h>
#include <libweston/windowed-output-api.h>
#include <xkbcommon/xkbcommon.h>
#include <mini-shell.h>

#define SOCK_NAME "wayland-weston-test"
#define OUT_W 640
#define OUT_H 480

/* 后端模块入口（libweston 正常走 dlopen；这里静态链接后直接调用） */
extern int weston_backend_init(struct weston_compositor *compositor,
                               struct weston_backend_config *config_base);

/* eggs/weston/compat/backend-access.c：取第一个已注册后端（公开头无迭代 API） */
extern struct weston_backend *
weston_compositor_get_first_backend(struct weston_compositor *compositor);

static int g_pass;
static int g_fail;

static void check(int cond, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#include <stdarg.h>
static void check(int cond, const char *fmt, ...) {
  va_list ap;
  char buf[512];

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (cond) {
    g_pass++;
    printf("TPASS: %s\n", buf);
  } else {
    g_fail++;
    printf("TFAIL: %s\n", buf);
  }
  fflush(stdout);
}

/* ===================== server ===================== */

/* 进度探针：串口输出在多进程早期可能丢，同步写文件最可靠 */
#define STEP_LOG "/tmp/wl-weston-child.log"

/* ready 管道只在 fork 模式下存在；server 单独跑时传 -1 */
static void ready_send(int fd, char c) {
  if (fd >= 0)
    write(fd, &c, 1);
}

static void step(const char *s) {
  int fd = open(STEP_LOG, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd >= 0) {
    write(fd, s, strlen(s));
    write(fd, "\n", 1);
    close(fd);
  }
  printf("SERVER: %s\n", s);
  fflush(stdout);
}

/* weston 日志直通 stdout（串口控制台） */
static void weston_log_print(const char *fmt, va_list ap) {
  vprintf(fmt, ap);
  fflush(stdout);
}

static int g_outputs_enabled;
static struct weston_output *g_output;

/* head 出现/变化时为其建输出并 enable（windowed 后端标准流程） */
static void on_heads_changed(struct wl_listener *listener, void *data) {
  struct weston_compositor *ec = data;
  const struct weston_windowed_output_api *api =
      weston_windowed_output_get_api(ec, WESTON_WINDOWED_OUTPUT_HEADLESS);
  struct weston_head *head = NULL;

  (void)listener;

  while ((head = weston_compositor_iterate_heads(ec, head)) != NULL) {
    struct weston_output *out;

    if (!weston_head_is_connected(head) || weston_head_is_enabled(head))
      continue;

    out = weston_compositor_create_output(ec, head, weston_head_get_name(head));
    if (out == NULL) {
      step("create output failed");
      continue;
    }

    /* 窗口式后端要求先给定 scale（headless 的 output_set_size 里有
     * assert(output->base.current_scale)），再设尺寸。 */
    weston_output_set_scale(out, 1);
    weston_output_set_transform(out, WL_OUTPUT_TRANSFORM_NORMAL);

    if (api != NULL && api->output_set_size(out, OUT_W, OUT_H) < 0) {
      step("output_set_size failed");
      continue;
    }

    if (weston_output_enable(out) < 0) {
      step("output enable failed");
      continue;
    }

    /* 注意：这里【不能】立刻 weston_output_set_ready()。它会马上 schedule
     * 一次 repaint，而此刻场景图里还没有任何 view，compositor 会撞上
     * "empty scene graph at repaint" 断言退出。ready 由 mini-shell 在第一个
     * surface 入层之后再置位。 */
    g_outputs_enabled++;
    g_output = out;
    step("output enabled");

    if (mini_shell_init(ec, out) < 0)
      step("mini_shell_init failed");
  }
}

static struct wl_listener heads_listener = {
    .notify = on_heads_changed,
};

static int run_server(int ready_fd) {
  struct wl_display *display;
  struct weston_log_context *log_ctx;
  struct weston_compositor *ec;
  struct weston_backend *backend;
  const struct weston_windowed_output_api *api;
  struct weston_headless_backend_config bcfg;
  int rc = 1;

  step("start");

  /* weston 16 的日志走 log context（compositor_create 要求非 NULL）；
   * 这里挂一个直通 stdout 的 handler，方便在串口上看 weston 自己的诊断。 */
  weston_log_set_handler(weston_log_print, weston_log_print);
  log_ctx = weston_log_ctx_create();
  if (log_ctx == NULL) {
    step("weston_log_ctx_create failed");
    ready_send(ready_fd, 'E');
    return 1;
  }

  /* wl_display_add_socket 要求 XDG_RUNTIME_DIR；本系统惯例是 /tmp */
  if (setenv("XDG_RUNTIME_DIR", "/tmp", 1) != 0)
    step("setenv failed");
  step("env ok");

  display = wl_display_create();
  if (display == NULL) {
    step("wl_display_create failed");
    ready_send(ready_fd, 'E');
    return 1;
  }
  step("wl_display_create ok");

  ec = weston_compositor_create(display, log_ctx, NULL, NULL);
  if (ec == NULL) {
    step("weston_compositor_create failed");
    ready_send(ready_fd, 'E');
    return 1;
  }
  step("compositor created");

  /* 建 XKB 上下文与 RMLVO 缺省值（evdev/pc105/us）。不做这步，后面
   * headless 的 fake seat 建键盘时会拿 NULL context 去编 keymap 而崩。 */
  if (weston_compositor_set_xkb_rule_names(ec, NULL) < 0) {
    step("set xkb rule names failed");
    ready_send(ready_fd, 'E');
    return 1;
  }
  step("xkb context ready");

  memset(&bcfg, 0, sizeof(bcfg));
  bcfg.base.struct_version = WESTON_HEADLESS_BACKEND_CONFIG_VERSION;
  bcfg.base.struct_size = sizeof(bcfg);
  bcfg.renderer = WESTON_RENDERER_PIXMAN;
  bcfg.fake_seat = true;
  bcfg.refresh = 60000; /* mHz → 60Hz */
  bcfg.output_fb_alpha_encoding = WESTON_OUTPUT_FB_ALPHA_PREMULT;

  if (weston_backend_init(ec, (struct weston_backend_config *)&bcfg) < 0) {
    step("headless backend init failed");
    ready_send(ready_fd, 'E');
    return 1;
  }
  step("headless backend up");

  /* 后端就绪后必须收尾：选定 presentation clock、装默认 color manager
   * （noop）。不做这步，后面建 output 会在 compositor->color_manager 空指针
   * 上调 vtable 而崩。 */
  if (weston_compositor_backends_loaded(ec) < 0) {
    step("backends_loaded failed");
    ready_send(ready_fd, 'E');
    return 1;
  }
  step("backends loaded");

  /* 输出不是直接 create 出来的：windowed 后端的标准做法是先建 head
   * （api->create_head），head 挂进 compositor 后触发 heads_changed 信号，
   * 在回调里为每个 head 建 output、设尺寸并 enable。 */
  weston_compositor_add_heads_changed_listener(ec, &heads_listener);

  api = weston_windowed_output_get_api(ec, WESTON_WINDOWED_OUTPUT_HEADLESS);
  if (api == NULL) {
    step("windowed output api missing");
    ready_send(ready_fd, 'E');
    return 1;
  }

  backend = weston_compositor_get_first_backend(ec);
  if (backend == NULL) {
    step("no backend registered");
    ready_send(ready_fd, 'E');
    return 1;
  }

  if (api->create_head(backend, "headless") < 0) {
    step("create_head failed");
    ready_send(ready_fd, 'E');
    return 1;
  }
  step("head created");

  /* 后端的 add_head 只是把 head 挂上并排队一次 heads_changed 通知（idle 里发），
   * 这里主动 flush 一次，立刻走我们的回调把输出建起来并 enable。 */
  weston_compositor_flush_heads_changed(ec);

  if (g_outputs_enabled == 0) {
    step("no output enabled");
    ready_send(ready_fd, 'E');
    return 1;
  }

  weston_compositor_wake(ec);

  if (wl_display_add_socket(display, SOCK_NAME) != 0) {
    step("add socket failed");
    ready_send(ready_fd, 'E');
    return 1;
  }
  step("socket listening");
  ready_send(ready_fd, 'R');

  wl_display_run(display);

  rc = 0;
  wl_display_destroy_clients(display);
  weston_compositor_destroy(ec);
  wl_display_destroy(display);
  return rc;
}

/* ===================== client ===================== */

struct globals {
  int compositor;
  int subcompositor;
  int shm;
  int output;
  int viewporter;
  int presentation;
  int xdg_output_manager;
  int seat;
  int total;

  /* 画像素要用到的两个对象 */
  struct wl_compositor *compositor_obj;
  struct wl_shm *shm_obj;
};

static void on_global(void *data, struct wl_registry *reg, uint32_t name,
                      const char *iface, uint32_t version) {
  struct globals *g = data;

  (void)version;
  g->total++;

  if (strcmp(iface, "wl_compositor") == 0) {
    g->compositor++;
    g->compositor_obj =
        wl_registry_bind(reg, name, &wl_compositor_interface, 4);
  } else if (strcmp(iface, "wl_subcompositor") == 0)
    g->subcompositor++;
  else if (strcmp(iface, "wl_shm") == 0) {
    g->shm++;
    g->shm_obj = wl_registry_bind(reg, name, &wl_shm_interface, 1);
  }
  else if (strcmp(iface, "wl_output") == 0)
    g->output++;
  else if (strcmp(iface, "wp_viewporter") == 0)
    g->viewporter++;
  else if (strcmp(iface, "wp_presentation") == 0)
    g->presentation++;
  else if (strcmp(iface, "zxdg_output_manager_v1") == 0)
    g->xdg_output_manager++;
  else if (strcmp(iface, "wl_seat") == 0)
    g->seat++;
}

static const struct wl_registry_listener registry_listener = {
    .global = on_global,
};

static int run_client(void) {
  struct wl_display *display;
  struct wl_registry *registry;
  struct globals g;
  int i;

  step("client start");

  /* server 可能还在起，重试几次 */
  for (i = 0; i < 50; i++) {
    display = wl_display_connect(SOCK_NAME);
    if (display != NULL)
      break;
    usleep(100 * 1000);
  }
  if (display == NULL) {
    step("client connect failed");
    g_fail++;
    return 1;
  }
  step("client connected");

  memset(&g, 0, sizeof(g));
  registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, &g);

  step("client roundtrip...");
  if (wl_display_roundtrip(display) < 0) {
    step("client roundtrip failed");
    check(0, "wl_display_roundtrip 返回 %d", wl_display_get_error(display));
    return 1;
  }
  step("client roundtrip done");
  check(1, "wl_display_roundtrip 完成（server 事件循环存活）");

  check(g.compositor == 1, "wl_compositor global 已注册");
  check(g.subcompositor == 1, "wl_subcompositor global 已注册");
  check(g.shm == 1, "wl_shm global 已注册");
  check(g.output >= 1, "wl_output global 已注册（headless 输出 enable 成功）");
  check(g.viewporter == 1, "wp_viewporter global 已注册");
  check(g.presentation == 1, "wp_presentation global 已注册");
  check(g.xdg_output_manager == 1, "zxdg_output_manager_v1 global 已注册");
  check(g.total >= 7, "registry 共 %d 个 global", g.total);

  /* ---- 画一块 64x64 纯红，走 wl_shm + attach/commit ---- *
   * 像素是否真的进了 output 影子缓冲，由 server 侧（mini-shell 的定时抓帧）
   * 打印 SHELL: 行给出；client 这边负责把 buffer 递出去并确认 server 消费。 */
  {
    const int W = 64;
    const int H = 64;
    const int stride = W * 4;
    const int size = stride * H;
    struct wl_shm_pool *pool;
    struct wl_buffer *buf;
    struct wl_surface *surf;
    uint32_t *px;
    int fd;
    int i;

    fd = memfd_create("wl-weston-shm", 0);
    check(fd >= 0, "memfd_create 返回 fd=%d", fd);
    if (fd < 0) {
      wl_display_disconnect(display);
      return 1;
    }

    check(ftruncate(fd, size) == 0, "ftruncate(%d) 成功（%d 字节）", fd, size);

    px = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    check(px != MAP_FAILED, "mmap 共享页 %d 字节", size);
    if (px == MAP_FAILED) {
      close(fd);
      wl_display_disconnect(display);
      return 1;
    }

    for (i = 0; i < W * H; i++)
      px[i] = 0xFFFF0000u; /* XRGB8888 纯红 */

    surf = wl_compositor_create_surface(g.compositor_obj);
    pool = wl_shm_create_pool(g.shm_obj, fd, size);
    buf = wl_shm_pool_create_buffer(pool, 0, W, H, stride,
                                    WL_SHM_FORMAT_XRGB8888);
    check(surf != NULL && pool != NULL && buf != NULL,
          "surface/pool/buffer 创建成功");

    if (surf != NULL && buf != NULL) {
      wl_surface_attach(surf, buf, 0, 0);
      wl_surface_damage(surf, 0, 0, W, H);
      wl_surface_commit(surf);
      step("client committed 64x64 red");

      if (wl_display_roundtrip(display) < 0)
        check(0, "commit 后 roundtrip 失败");
      else
        check(1, "commit 后 roundtrip 成功（server 已消费请求）");
    }

    /* 留 300ms 给 server 侧 mini-shell 的定时抓帧打印 SHELL: 行 */
    usleep(300 * 1000);
  }

  wl_registry_destroy(registry);
  wl_display_disconnect(display);
  return 0;
}

/* ===================== xkb 自检 ===================== */

static int run_xkb(void) {
  struct xkb_context *ctx;
  struct xkb_keymap *map;
  struct xkb_rule_names names = {0};

  names.rules = "evdev";
  names.model = "pc105";
  names.layout = "us";
  names.variant = "";
  names.options = "";

  ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  check(ctx != NULL, "xkb_context_new() 返回 %p", (void *)ctx);
  if (ctx == NULL) {
    ctx = xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES);
    check(ctx != NULL, "xkb_context_new(NO_DEFAULT_INCLUDES) 返回 %p", (void *)ctx);
  }
  if (ctx == NULL)
    return 1;

  map = xkb_keymap_new_from_names(ctx, &names, 0);
  check(map != NULL, "xkb_keymap_new_from_names(evdev/pc105/us) 返回 %p",
        (void *)map);

  if (map == NULL) {
    /* 没有 xkeyboard-config 数据时，退回内嵌的最小 keymap 文本 */
    static const char minimal[] =
        "xkb_keymap {\n"
        "  xkb_keycodes \"(evdev)\" { <ESC> = 9; <AE01> = 10; <AD01> = 24;"
        " <AC01> = 38; <AB01> = 52; <TLDE> = 49; <BKSP> = 22; <TAB> = 23;"
        " <RTRN> = 36; <LFSH> = 50; <SPCE> = 65; };\n"
        "  xkb_types \"(default)\" { include \"basic\" };\n"
        "  xkb_compatibility \"(default)\" { include \"basic\" };\n"
        "  xkb_symbols \"(default)\" {\n"
        "    key <TLDE> { [ grave, asciitilde ] };\n"
        "    key <AE01> { [ 1, exclam ] };\n"
        "    key <AD01> { [ q, Q ] };\n"
        "    key <AC01> { [ a, A ] };\n"
        "    key <AB01> { [ z, Z ] };\n"
        "    key <SPCE> { [ space ] };\n"
        "    key <RTRN> { [ Return ] };\n"
        "    key <BKSP> { [ BackSpace ] };\n"
        "    key <TAB> { [ Tab ] };\n"
        "    key <ESC> { [ Escape ] };\n"
        "    key <LFSH> { [ Shift_L ] };\n"
        "  };\n"
        "};\n";

    map = xkb_keymap_new_from_string(ctx, minimal,
                                     XKB_KEYMAP_FORMAT_TEXT_V1,
                                     XKB_KEYMAP_COMPILE_NO_FLAGS);
    check(map != NULL, "内嵌最小 keymap 编译 %p", (void *)map);
  }

  if (map != NULL) {
    struct xkb_state *st = xkb_state_new(map);
    check(st != NULL, "xkb_state_new 成功");
    if (st != NULL) {
      xkb_state_update_key(st, 38 /* <AC01>=a */, XKB_KEY_DOWN);
      check(1, "xkb_state_update_key 不崩");
      xkb_state_unref(st);
    }
    xkb_keymap_unref(map);
  }

  xkb_context_unref(ctx);
  printf("\nSummary:\npassed %d\nfailed %d\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}

/* ===================== main ===================== */

int main(int argc, char **argv) {
  int fds[2];
  pid_t pid;
  int server_rc;
  int client_rc;
  int st;

  setbuf(stdout, NULL);

  /* wayland 的 socket 目录约定：/tmp（本系统没有桌面会话环境） */
  setenv("XDG_RUNTIME_DIR", "/tmp", 1);

  /* 两种跑法：
   *   wl-weston             —— fork 出 server + client（默认，便于一条命令自测）
   *   wl-weston server      —— 只跑 server（前台阻塞，便于 shell 里 & 起）
   *   wl-weston client      —— 只跑 client（连已经起来的 server）
   * 拆分模式用于排查：本内核 fork 大二进制时子进程存活有问题。 */
  if (argc > 1 && strcmp(argv[1], "server") == 0)
    return run_server(-1);
  if (argc > 1 && strcmp(argv[1], "client") == 0)
    return run_client();
  if (argc > 1 && strcmp(argv[1], "xkb") == 0)
    return run_xkb();

  /* 后台常驻：父进程立刻返回（shell 拿到提示符），子进程跑 server。
   * 用于"一条命令起 server、下一条命令跑 client"的分步验证。 */
  if (argc > 1 && strcmp(argv[1], "daemon") == 0) {
    pid_t d = fork();
    if (d < 0)
      return 1;
    if (d > 0) {
      printf("DAEMON: pid=%d\n", (int)d);
      return 0;
    }
    setsid();
    return run_server(-1);
  }

  if (pipe(fds) != 0) {
    perror("pipe");
    return 1;
  }

  pid = fork();
  if (pid < 0) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    /* 子进程：跑 server，结束就 _exit（不要回到父进程流程） */
    close(fds[0]);
    run_server(fds[1]);
    _exit(0);
  }

  /* 父进程：不再等 pipe（阻塞读在这个内核上表现不可靠），
   * 直接用 run_client() 里的连接重试来等 server listen。 */
  close(fds[1]);
  close(fds[0]);

  client_rc = run_client();

  if (client_rc != 0) {
    /* client 失败时把 server 子进程的死法说清楚（早退/被信号/还活着） */
    int wr = waitpid(pid, &st, WNOHANG);

    if (wr == pid) {
      if (WIFEXITED(st))
        printf("TINFO: server 子进程已退出，exit code=%d\n", WEXITSTATUS(st));
      else if (WIFSIGNALED(st))
        printf("TINFO: server 子进程被信号 %d 杀死\n", WTERMSIG(st));
    } else {
      printf("TINFO: server 子进程仍在运行（client 连不上是 socket 层问题）\n");
    }
  }

  /* 收尾：先 SIGTERM（验证内核"默认动作=终止"是否生效），200ms 内没退出
   * 再补 SIGKILL 兜底（否则会永久卡在 waitpid）。 */
  {
    int waited = 0;
    int k;

    kill(pid, SIGTERM);
    for (k = 0; k < 20; k++) {
      if (waitpid(pid, &st, WNOHANG) == pid) {
        waited = 1;
        break;
      }
      usleep(10 * 1000);
    }
    if (!waited) {
      kill(pid, SIGKILL);
      waitpid(pid, &st, 0);
      printf("TINFO: SIGTERM 未终止 server，已用 SIGKILL 兜底\n");
    }
  }
  server_rc = WIFEXITED(st) ? WEXITSTATUS(st) : 1;

  check(client_rc == 0, "client 阶段全部检查完成");
  check(server_rc == 0 || WIFSIGNALED(st), "server 正常退出（rc=%d sig=%d）",
        server_rc, WIFSIGNALED(st) ? WTERMSIG(st) : 0);

  printf("\nSummary:\npassed %d\nfailed %d\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
