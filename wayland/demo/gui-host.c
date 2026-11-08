/* wl-gui-host - 最小 libgui 应用：只调 libgui 的 screen API。
 *
 * Wayland 窗口、wl_shm、提交全部由 libgui 的 SCREEN_MODE_WAYLAND 后端
 * （eggs/libgui/screen_wayland.c）完成，这里没有任何 Wayland/EGL 代码。
 */
#include <stdio.h>
#include <unistd.h>

#include "screen.h"

static void draw(int frame) {
  screen_info_t *s = screen_info();
  int x = (frame * 4) % (s->width - 120 + 1);
  int y = s->height - 90 - (frame * 2) % (s->height - 100);
  screen_fill_rect(0, 0, s->width, s->height, 0xff1a1a2e);
  screen_fill_rect(x, y, 120, 80, 0xffe94560);
  screen_fill_rect(x + 20, y + 20, 80, 40, 0xff0f3460);
  screen_draw_string(20, 20, "libgui on Wayland");
  screen_printf(20, 44, "frame %d  %dx%d", frame, s->width, s->height);
}

int main(void) {
  int frame = 0;
  screen_set_size(640, 480);
  /* 默认 Wayland，但尊重 YIYIYA_GUI=fb/xwin/xwin_buffer/wayland。 */
  screen_init_with_mode(screen_mode_from_env(SCREEN_MODE_WAYLAND));
  screen_set_title("libgui");
  printf("gui-host: canvas %dx%d\n", screen_info()->width,
         screen_info()->height);
  for (;;) {
    draw(frame++);
    /* libgui 内部：复制画布 -> wl_shm -> wl_surface_commit */
    screen_flush();
    usleep(16000);
  }
  return 0;
}
