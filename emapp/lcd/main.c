/*******************************************************************
 * Copyright 2021-present evilbinary
 * 作者: evilbinary on 01/01/20
 * 邮箱: rootdebug@163.com
 ********************************************************************/
#include "main.h"
#include "lcd_draw.h"

#define LCD_W 128
#define LCD_H 128

#define BLACK 0x0000
#define WHITE 0xFFFF
#define RED 0xF800
#define BLUE 0x001F
#define GREEN 0x07E0
#define YELLOW 0xFFE0
#define MAGENTA 0xF81F
#define CYAN 0xFFE0
#define ORANGE 0xFD20
#define GRAY 0x8410
#define DARK 0x2104

typedef enum {
  MENU_BRIGHTNESS = 0,
  MENU_CCT = 1,
  MENU_BATTERY = 2,
  MENU_CLOCK = 3,
  MENU_COUNT
} menu_id_t;

typedef struct lamp_ui_state {
  int brightness;
  int cct;
  int battery;
  int charging;
  int selected;
  int hour;
  int minute;
  int second;
} lamp_ui_state_t;

typedef struct lamp_ui_cache {
  int valid;
} lamp_ui_cache_t;

extern int module_ready;
extern void do_kernel_thread();

void kstart(int argc, char* argv[], char** envp) {
  boot_info_t* boot_info = envp[0];
  // int cpu = envp[1];
  int cpu = cpu_get_id();
  arch_init(boot_info, cpu);

  kmain(argc, argv);
   
  for (;;) {
    cpu_halt();
  }
}




void sleep_ms(int ms) {
  struct timespec tv;
  tv.tv_nsec = (ms % 1000) * 1000 * 1000;
  tv.tv_sec = ms / 1000;
  syscall4(SYS_CLOCK_NANOSLEEP, 0, 0, &tv, &tv);
}

static int clamp_i(int value, int min, int max) {
  if (value < min) return min;
  if (value > max) return max;
  return value;
}

static int ui_value_to_width(int value, int max_value, int width) {
  if (max_value <= 0) return 0;
  value = clamp_i(value, 0, max_value);
  return (value * width) / max_value;
}

static void ui_format_time(char* buf, lamp_ui_state_t* ui) {
  sprintf(buf, "%02d:%02d", ui->hour, ui->minute);
}

static void ui_draw_bar(int x, int y, int w, int h, int fill_w, u16 fg,
                        u16 bg, u16 border) {
  lcd_fill_rect(x, y, w, h, bg);
  if (fill_w > 0) {
    lcd_fill_rect(x, y, fill_w, h, fg);
  }
  lcd_draw_rect(x - 1, y - 1, w + 2, h + 2, border);
}

static void ui_draw_header_static(void) {
  lcd_fill_rect(0, 0, LCD_W, 18, BLACK);
  lcd_draw_text_bg(4, 4, WHITE, BLACK, 8, "PHOTO LIGHT");
}

static void ui_draw_header_dynamic(lamp_ui_state_t* ui) {
  char buf[32];
  ui_format_time(buf, ui);
  lcd_fill_rect(88, 2, 36, 14, BLACK);
  lcd_draw_text_bg(90, 4, YELLOW, BLACK, 8, buf);
}

static void ui_draw_battery(int x, int y, int percent, int charging) {
  char buf[16];
  int level_w = ui_value_to_width(percent, 100, 18);

  lcd_draw_rect(x, y, 22, 10, WHITE);
  lcd_fill_rect(x + 22, y + 3, 2, 4, WHITE);
  lcd_fill_rect(x + 2, y + 2, 18, 6, DARK);
  if (level_w > 0) {
    lcd_fill_rect(x + 2, y + 2, level_w, 6, charging ? GREEN : CYAN);
  }

  sprintf(buf, "%d%%", percent);
  lcd_draw_text_bg(x - 2, y + 12, WHITE, BLACK, 8, buf);
}

static void ui_draw_status_card_static(int x, int y, int w, int h,
                                       const char* title) {
  lcd_fill_rect(x, y, w, h, BLACK);
  lcd_draw_rect(x, y, w, h, GRAY);
  lcd_draw_text_bg(x + 4, y + 4, WHITE, BLACK, 8, title);
}

static void ui_draw_dashboard_static(void) {
  ui_draw_status_card_static(4, 22, 56, 30, "BRI");
  ui_draw_status_card_static(68, 22, 56, 30, "CCT");
  ui_draw_status_card_static(4, 56, 56, 30, "BAT");
  ui_draw_status_card_static(68, 56, 56, 30, "TIME");
}

static void ui_draw_status_value(int x, int y, int w, const char* value,
                                 int selected, u16 accent) {
  lcd_fill_rect(x - 2, y - 2, w + 4, 13, BLACK);
  if (selected) {
    lcd_draw_rect(x - 2, y - 2, w + 4, 13, accent);
  }
  lcd_draw_text_bg(x, y, WHITE, BLACK, 8, value);
}

static void ui_draw_dashboard_dynamic(lamp_ui_state_t* ui) {
  char buf[32];

  sprintf(buf, "%d%%", ui->brightness);
  ui_draw_status_value(8, 40, 40, buf, ui->selected == MENU_BRIGHTNESS, YELLOW);

  sprintf(buf, "%dK", ui->cct);
  ui_draw_status_value(72, 40, 40, buf, ui->selected == MENU_CCT, ORANGE);

  sprintf(buf, "%d%%", ui->battery);
  ui_draw_status_value(8, 74, 40, buf, ui->selected == MENU_BATTERY, GREEN);

  ui_format_time(buf, ui);
  ui_draw_status_value(72, 74, 40, buf, ui->selected == MENU_CLOCK, CYAN);
}

static void ui_draw_menu_static(void) {
  static const char* menu_labels[MENU_COUNT] = {"Brightness", "Color Temp",
                                                "Battery", "Clock"};
  int y = 92;

  lcd_draw_text_bg(4, y, WHITE, BLACK, 8, "MENU");
  for (int i = 0; i < MENU_COUNT; i++) {
    int row_y = y + 10 + i * 8;
    lcd_draw_text_bg(4, row_y, GRAY, BLACK, 8, menu_labels[i]);
  }
}

static void ui_draw_menu_dynamic(lamp_ui_state_t* ui) {
  static const char* menu_labels[MENU_COUNT] = {"Brightness", "Color Temp",
                                                "Battery", "Clock"};
  int y = 92;
  for (int i = 0; i < MENU_COUNT; i++) {
    int row_y = y + 10 + i * 8;
    u16 color = (i == ui->selected) ? CYAN : GRAY;
    lcd_fill_rect(4, row_y, 72, 8, BLACK);
    lcd_draw_text_bg(4, row_y, color, BLACK, 8, menu_labels[i]);
  }
}

static void ui_draw_focus_panel_static(void) {
  lcd_fill_rect(80, 90, 44, 34, BLACK);
  lcd_draw_rect(80, 90, 44, 34, WHITE);
}

static void ui_draw_focus_panel_dynamic(lamp_ui_state_t* ui) {
  char buf[32];

  lcd_fill_rect(84, 94, 34, 24, BLACK);
  if (ui->selected == MENU_BRIGHTNESS) {
    lcd_draw_text_bg(84, 94, YELLOW, BLACK, 8, "LEVEL");
    sprintf(buf, "%d%%", ui->brightness);
    lcd_draw_text_bg(84, 104, WHITE, BLACK, 8, buf);
    ui_draw_bar(84, 114, 34, 4, ui_value_to_width(ui->brightness, 100, 34),
                YELLOW, DARK, YELLOW);
  } else if (ui->selected == MENU_CCT) {
    lcd_draw_text_bg(84, 94, ORANGE, BLACK, 8, "TEMP");
    sprintf(buf, "%dK", ui->cct);
    lcd_draw_text_bg(84, 104, WHITE, BLACK, 8, buf);
    ui_draw_bar(84, 114, 34, 4, ui_value_to_width(ui->cct - 2700, 3800, 34),
                ORANGE, DARK, ORANGE);
  } else if (ui->selected == MENU_BATTERY) {
    lcd_draw_text_bg(84, 94, GREEN, BLACK, 8, "POWER");
    sprintf(buf, "%d%%", ui->battery);
    lcd_draw_text_bg(84, 104, WHITE, BLACK, 8, buf);
    ui_draw_bar(84, 114, 34, 4, ui_value_to_width(ui->battery, 100, 34),
                GREEN, DARK, GREEN);
  } else {
    ui_format_time(buf, ui);
    lcd_draw_text_bg(84, 94, CYAN, BLACK, 8, "CLOCK");
    lcd_draw_text_bg(84, 104, WHITE, BLACK, 8, buf);
    ui_draw_bar(84, 114, 34, 4, ui_value_to_width(ui->minute, 59, 34), CYAN,
                DARK, CYAN);
  }
}

static void ui_draw_static(void) {
  lcd_fill_rect(0, 0, LCD_W, LCD_H, BLACK);
  ui_draw_header_static();
  ui_draw_dashboard_static();
  ui_draw_menu_static();
  ui_draw_focus_panel_static();
}

static void ui_draw_dynamic(lamp_ui_state_t* ui) {
  ui_draw_header_dynamic(ui);
  ui_draw_battery(96, 2, ui->battery, ui->charging);
  ui_draw_dashboard_dynamic(ui);
  ui_draw_menu_dynamic(ui);
  ui_draw_focus_panel_dynamic(ui);
}

static void ui_render(lamp_ui_state_t* ui, lamp_ui_cache_t* cache) {
  if (!cache->valid) {
    ui_draw_static();
    cache->valid = 1;
  }
  ui_draw_dynamic(ui);
}

static void ui_tick(lamp_ui_state_t* ui) {
  ui->second++;
  if (ui->second >= 60) {
    ui->second = 0;
    ui->minute++;
  }
  if (ui->minute >= 60) {
    ui->minute = 0;
    ui->hour++;
  }
  if (ui->hour >= 24) {
    ui->hour = 0;
  }
}

static void ui_demo_update(lamp_ui_state_t* ui, int frame) {
  ui->selected = (frame / 10) % MENU_COUNT;
  ui->brightness = 35 + ((frame * 7) % 60);
  ui->cct = 3200 + ((frame * 120) % 2500);
  ui->battery = 95 - ((frame / 4) % 40);
  ui->charging = ((frame / 16) % 2) == 0;
  ui_tick(ui);
}

void thread_lcd(){
  lamp_ui_state_t ui = {
      .brightness = 72,
      .cct = 5600,
      .battery = 86,
      .charging = 0,
      .selected = MENU_BRIGHTNESS,
      .hour = 18,
      .minute = 30,
      .second = 0,
  };
  lamp_ui_cache_t cache = {0};
  int frame = 0;

  while (module_ready <= 0) {
  }

  while (1) {
    ui_demo_update(&ui, frame++);
    ui_render(&ui, &cache);
    sleep_ms(250);
  }
}


void thread_lcd2(){
  while (module_ready <= 0) {
    // sleep();
  }
  int i=0;
  while(1){
    sleep_ms(1000);
  }
}


void do_kernel_thread(void) {
  kprintf("init kernel thread\n");
  modules_init();
  mp_init();

  module_ready = 1;
  
  u32 i = 0;
  u32 count = 0;
  for (;;) {
    count++;
    if (i % 4 == 0) {
      i = 0;
    }
    // log_debug("count=%d\n",count);
    // test_fb(count);
    schedule_sleep(1000 * 1000 * 10000);
    // cpu_wait();
  }
}

int kmain(int argc, char* argv[]) {
  kernel_init();

  log_info("kernel thread init\n");

  thread_t* t1 = thread_create_name_level("kernel", (void*)&do_kernel_thread,
                                          NULL, LEVEL_KERNEL_SHARE);
  thread_t* t2 = thread_create_name("lcd", (void*)&thread_lcd, NULL);
  thread_run(t1);
  thread_run(t2);

  thread_t* t3 = thread_create_name("lcd2", (void*)&thread_lcd2, NULL);
  thread_run(t3);


  log_info("kernel run start\n");

  kernel_run();

  return 0;
}