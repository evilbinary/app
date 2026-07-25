#include "event.h"
#include "jpeg_decoder.h"
#include "screen.h"
#include "stdio.h"
#include "syscall.h"
#include "time.h"

char* buf = "hello,gui\n";

void* load_bmp(char* name) {
  char* buffer = malloc(200 * 1024);
  if (buffer == NULL) {
    printf("load_bmp: malloc failed for %s\n", name);
    return NULL;
  }
  memset(buffer, 0, 200 * 1024);
  FILE* fp;
  fp = fopen(name, "rb");
  if (fp == NULL) {
    printf("load_bmp: open failed for %s\n", name);
    free(buffer);
    return NULL;
  }
  // printf("fd=%d\n", fp->fd);
  u32 offset = 0;
  for (;;) {
    fseek(fp, offset, SEEK_SET);
    u32 ret = fread(buffer + offset, 1, 512, fp);
    if (ret <= 0) {
      break;
    }
    offset++;
  }
  // printf("read end=%d\n", fp->fd);
  fclose(fp);
  return buffer;
}

void display_time() {
  char buffer[80];
  struct tm* timeinfo;
  time_t lt;
  time(&lt);
  timeinfo = localtime(&lt);
  strftime(buffer, 80, "%Y-%m-%d %I:%M:%S", timeinfo);
  screen_printf(840, 10, "%s\n", buffer);
}

void yiyiya_gui() {
  int fd = open("/dev/mouse", 0);
  printf("mouse fd %d\n", fd);
  if (fd < 0) {
    printf("gui: /dev/mouse not available, continuing without mouse\n");
  }
  screen_init();
  screen_info_t* screen = screen_info();
  if (screen == NULL) {
    printf("screen_info failed\n");
    return;
  }

  bitmap_t* bitmap = load_jpeg("/res/home.jpg");
  bitmap_t* bitmap2 = load_png("/res/normal.png");
  if (bitmap == NULL) {
    printf("load_jpeg failed: /res/home.jpg\n");
  }
  if (bitmap2 == NULL) {
    printf("load_png failed: /res/normal.png\n");
  }

  mouse_data_t mouse;
  memset(&mouse, 0, sizeof(mouse));

  screen_fill_rect(0, 0, screen->width, screen->height, 0xff0000);
  for (;;) {
    if (bitmap != NULL) {
      screen_show_bitmap(0, 0, 1024, 768, bitmap);
    }

    screen_printf(500, 10, "YiYiYa OS");
    event_read_mouse(&mouse);
    screen_printf(10, 100, "mouse=%d,%d", mouse.x, mouse.y);
    screen_fill_rect(mouse.x, screen->height - mouse.y, 4, 4, 0x00ff00);

    screen_flush();
  }
}

void yiyiya_display() {
  screen_info_t* screen = screen_info();
  if (screen == NULL || screen->width == 0 || screen->buffer == NULL) {
    printf("screen_init\n");
    screen_init();
    printf("screen_init end\n");
    screen = screen_info();
  }
  int i = 0;
  u32 w = screen != NULL && screen->width > 0 ? screen->width : 480;
  u32 h = screen != NULL && screen->height > 0 ? screen->height : 320;
  for (;;) {
    screen_fill_rect(0, 0, w, h, 0xffff0000);
    screen_printf(0, 0, "hello display YiYiYa Os %d\n", i);
    screen_flush();
    i++;
    {
      struct timespec tv;
      tv.tv_sec = 0;
      tv.tv_nsec = 16 * 1000 * 1000;
      nanosleep(&tv, NULL);
    }
  }
}

void yiyiya_bitmap() {
  screen_init();
  // bitmap_t* bitmap = load_jpeg("/home.jpg");
  bitmap_t* bitmap = load_jpeg("/duck.jpg");
  if (bitmap == NULL) {
    printf("load_jpeg failed: /duck.jpg\n");
    return;
  }
  //  bitmap_t* bitmap = load_png("/normal.png");
  // void* bmp = load_bmp("/bomb.bmp");
  for (;;) {
    screen_show_bitmap(0, 0, 227, 149, bitmap);
    // screen_show_bitmap(0, 0, 1024, 768, bitmap);
    // screen_show_bitmap(0, 0, 640, 480, bitmap);
    // screen_show_bmp_picture(200, 200, bmp, 0, 0);
    screen_flush();
  }
}

int main(int argc, char* argv[]) {
  printf(buf);
  if (argc > 1 && argv[1] != NULL && argv[1][0] == 'f') {
    printf("screen_init fb\n");
    screen_init_with_mode(SCREEN_MODE_FB);
    printf("screen_init end\n");
  } else {
    printf("screen_init\n");
    screen_init();
    printf("screen_init end\n");
  }
  // yiyiya_gui();
  yiyiya_display();
  // yiyiya_bitmap();

  return 0;
}