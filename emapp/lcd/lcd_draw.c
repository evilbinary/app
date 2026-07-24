#include "lcd_draw.h"

static int g_lcd_fd = -1;

int lcd_open(void) {
  if (g_lcd_fd >= 0) {
    return g_lcd_fd;
  }
  g_lcd_fd = syscall2(SYS_OPEN, "/dev/lcd", 0);
  if (g_lcd_fd < 0) {
    kprintf("open /dev/lcd failed!\n");
  }
  return g_lcd_fd;
}

static void lcd_write_cmd(const char* cmd) {
  if (lcd_open() < 0) {
    return;
  }
  syscall3(SYS_WRITE, g_lcd_fd, (void*)cmd, kstrlen(cmd));
}

void lcd_fill_rect(int x, int y, int w, int h, u16 color) {
  char cmd_buf[64];
  sprintf(cmd_buf, "FILL %d %d %d %d %d\n", x, y, w, h, color);
  lcd_write_cmd(cmd_buf);
}

void lcd_draw_pixel(int x, int y, u16 color) {
  char cmd_buf[48];
  sprintf(cmd_buf, "PIXEL %d %d %d\n", x, y, color);
  lcd_write_cmd(cmd_buf);
}

void lcd_draw_line(int x1, int y1, int x2, int y2, u16 color) {
  char cmd_buf[64];
  sprintf(cmd_buf, "LINE %d %d %d %d %d\n", x1, y1, x2, y2, color);
  lcd_write_cmd(cmd_buf);
}

void lcd_draw_rect(int x, int y, int w, int h, u16 color) {
  char cmd_buf[64];
  sprintf(cmd_buf, "RECT %d %d %d %d %d\n", x, y, w, h, color);
  lcd_write_cmd(cmd_buf);
}

void lcd_draw_text(int x, int y, u16 color, int size, const char* text) {
  char cmd_buf[128];
  sprintf(cmd_buf, "TEXT %d %d %d %d %s\n", x, y, color, size, text);
  lcd_write_cmd(cmd_buf);
}

void lcd_draw_text_bg(int x, int y, u16 color, u16 bg_color, int size,
                      const char* text) {
  char cmd_buf[128];
  sprintf(cmd_buf, "TEXTBG %d %d %d %d %d %s\n", x, y, color, bg_color, size,
          text);
  lcd_write_cmd(cmd_buf);
}
