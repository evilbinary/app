/*******************************************************************
 * Copyright 2021-present evilbinary
 * 作者: evilbinary on 01/01/20
 * 邮箱: rootdebug@163.com
 * X Window Demo Application
 ********************************************************************/
#include "kernel/kernel.h"

#ifdef XWIN_MODULE
#include "modules/xwin/xwin.h"
#include "modules/vga/vga.h"

static xdisplay_t* demo_display = NULL;
static xwindow_t* demo_window = NULL;
static xwindow_t* demo_window2 = NULL;
static xwindow_t* theme_window = NULL;
static int demo_running = 0;
static int counter = 0;
static char counter_text[32];
static int current_theme = 0;

// 主题名称
static const char* theme_names[] = {
    "Dark",
    "Light", 
    "Blue",
    "Classic"
};

// 主题颜色预览
static u32 theme_colors[] = {
    0xFF3A3A3A,  // Dark
    0xFFF0F0F0,  // Light
    0xFF0078D4,  // Blue
    0xFF000080   // Classic
};

// 整数转字符串辅助函数
static void demo_int_to_str(char* buf, int val) {
    char tmp[16];
    int i = 0, neg = 0;
    if (val < 0) {
        neg = 1;
        val = -val;
    }
    if (val == 0) {
        tmp[i++] = '0';
    } else {
        while (val > 0) {
            tmp[i++] = (val % 10) + '0';
            val /= 10;
        }
    }
    int pos = 0;
    if (neg) {
        buf[pos++] = '-';
    }
    while (i > 0) {
        buf[pos++] = tmp[--i];
    }
    buf[pos] = '\0';
}

// 窗口事件回调
static void demo_window_event(xwindow_t* win, void* event) {
    xevent_t* evt = (xevent_t*)event;
    if (evt == NULL) return;
    
    // 标题栏高度（与主题一致）
    #define TITLE_BAR_HEIGHT 28

    switch (evt->type) {
        case XEVENT_MOUSE_MOVE:
            // 重绘窗口显示鼠标位置（内容从标题栏下方开始）
            xwin_clear(win);
            xwin_draw_rect(win, 5, TITLE_BAR_HEIGHT + 5, win->width - 10, 30, XCOLOR_BLUE);
            xwin_draw_text(win, 10, TITLE_BAR_HEIGHT + 12, "Mouse Position:", XCOLOR_WHITE);

            kstrcpy(counter_text, "X:");
            demo_int_to_str(counter_text + 2, evt->data.mouse.x);
            kstrcat(counter_text, " Y:");
            demo_int_to_str(counter_text + kstrlen(counter_text), evt->data.mouse.y);
            xwin_draw_text(win, 130, TITLE_BAR_HEIGHT + 12, counter_text, XCOLOR_YELLOW);
            xwin_damage_all(win);
            break;

        case XEVENT_MOUSE_DOWN:
            if (evt->data.mouse.button == XBUTTON_LEFT) {
                counter++;
                xwin_fill_rect(win, 20, TITLE_BAR_HEIGHT + 50, 100, 40, XCOLOR_GREEN);
                kstrcpy(counter_text, "Click: ");
                demo_int_to_str(counter_text + 7, counter);
                xwin_draw_text(win, 30, TITLE_BAR_HEIGHT + 60, counter_text, XCOLOR_WHITE);
                xwin_damage_all(win);
            } else if (evt->data.mouse.button == XBUTTON_RIGHT) {
                counter--;
                xwin_fill_rect(win, 20, TITLE_BAR_HEIGHT + 50, 100, 40, XCOLOR_RED);
                kstrcpy(counter_text, "Click: ");
                demo_int_to_str(counter_text + 7, counter);
                xwin_draw_text(win, 30, TITLE_BAR_HEIGHT + 60, counter_text, XCOLOR_WHITE);
                xwin_damage_all(win);
            }
            break;

        case XEVENT_KEY_DOWN:
            if (evt->data.key.keycode == 0x1B) {  // ESC
                demo_running = 0;
            }
            break;

        case XEVENT_CLOSE:
            demo_running = 0;
            break;

        default:
            break;
    }
}

// 窗口2事件回调 - 绘图板
static void demo_window2_event(xwindow_t* win, void* event) {
    xevent_t* evt = (xevent_t*)event;
    if (evt == NULL) return;

    static int last_x = -1, last_y = -1;
    static int drawing = 0;

    switch (evt->type) {
        case XEVENT_MOUSE_DOWN:
            if (evt->data.mouse.button == XBUTTON_LEFT) {
                drawing = 1;
                last_x = evt->data.mouse.x;
                last_y = evt->data.mouse.y;
                xwin_draw_pixel(win, last_x, last_y, XCOLOR_BLUE);
                xwin_damage_all(win);
            }
            break;

        case XEVENT_MOUSE_UP:
            if (evt->data.mouse.button == XBUTTON_LEFT) {
                drawing = 0;
                last_x = -1;
                last_y = -1;
            }
            break;

        case XEVENT_MOUSE_MOVE:
            if (drawing && last_x >= 0 && last_y >= 0) {
                xwin_draw_line(win, last_x, last_y,
                              evt->data.mouse.x, evt->data.mouse.y, XCOLOR_BLUE);
                xwin_damage_all(win);
                last_x = evt->data.mouse.x;
                last_y = evt->data.mouse.y;
            }
            break;

        case XEVENT_CLOSE:
            demo_running = 0;
            break;

        default:
            break;
    }
}

// 绘制主题窗口内容
static void draw_theme_window(xwindow_t* win) {
    #define THEME_TITLE_H 28
    #define THEME_BTN_W 80
    #define THEME_BTN_H 40
    #define THEME_BTN_GAP 10
    
    xwin_clear(win);
    
    // 标题
    xwin_draw_text(win, 70, THEME_TITLE_H + 5, "Select Theme", XCOLOR_WHITE);
    
    // 绘制4个主题按钮
    int start_x = 15;
    int start_y = THEME_TITLE_H + 30;
    
    for (int i = 0; i < 4; i++) {
        int btn_x = start_x + (i % 2) * (THEME_BTN_W + THEME_BTN_GAP);
        int btn_y = start_y + (i / 2) * (THEME_BTN_H + THEME_BTN_GAP);
        
        // 按钮背景色（使用主题预览色）
        u32 btn_color = theme_colors[i];
        if (i == current_theme) {
            // 当前主题用高亮边框
            xwin_fill_rect(win, btn_x - 2, btn_y - 2, THEME_BTN_W + 4, THEME_BTN_H + 4, XCOLOR_YELLOW);
        }
        xwin_fill_rect(win, btn_x, btn_y, THEME_BTN_W, THEME_BTN_H, btn_color);
        
        // 按钮文字
        u32 text_color = (i == 0 || i == 2 || i == 3) ? XCOLOR_WHITE : XCOLOR_BLACK;
        int text_x = btn_x + (THEME_BTN_W - 8 * kstrlen(theme_names[i])) / 2;
        int text_y = btn_y + (THEME_BTN_H - 8) / 2;
        xwin_draw_text(win, text_x, text_y, theme_names[i], text_color);
    }
    
    xwin_damage_all(win);
}

// 主题窗口事件回调
static void theme_window_event(xwindow_t* win, void* event) {
    xevent_t* evt = (xevent_t*)event;
    if (evt == NULL) return;
    
    #define THEME_TITLE_H 28
    #define THEME_BTN_W 80
    #define THEME_BTN_H 40
    #define THEME_BTN_GAP 10
    
    switch (evt->type) {
        case XEVENT_MOUSE_DOWN:
            if (evt->data.mouse.button == XBUTTON_LEFT) {
                int x = evt->data.mouse.x;
                int y = evt->data.mouse.y;
                
                // 检查是否点击了主题按钮
                int start_x = 15;
                int start_y = THEME_TITLE_H + 30;
                
                for (int i = 0; i < 4; i++) {
                    int btn_x = start_x + (i % 2) * (THEME_BTN_W + THEME_BTN_GAP);
                    int btn_y = start_y + (i / 2) * (THEME_BTN_H + THEME_BTN_GAP);
                    
                    if (x >= btn_x && x < btn_x + THEME_BTN_W &&
                        y >= btn_y && y < btn_y + THEME_BTN_H) {
                        // 切换主题
                        current_theme = i;
                        xtheme_set(demo_display, (xtheme_id_t)i);
                        xtheme_apply(demo_display);
                        
                        // 重绘主题窗口
                        draw_theme_window(win);
                        break;
                    }
                }
            }
            break;
            
        case XEVENT_CLOSE:
            demo_running = 0;
            break;
            
        default:
            break;
    }
}

// 初始化 xwin demo
int xwin_demo_init(void) {
    log_info("xwin_demo: initializing...\n");
    
    // 获取 VGA 设备
    device_t* vga_dev = device_find(DEVICE_VGA);
    if (vga_dev == NULL) {
        log_error("xwin_demo: No VGA device found\n");
        return -1;
    }
    
    // 初始化显示服务器
    demo_display = kmalloc(sizeof(xdisplay_t), KERNEL_TYPE);
    if (demo_display == NULL) {
        log_error("xwin_demo: Failed to allocate display\n");
        return -1;
    }
    
    int ret = xwin_init(demo_display, (vga_device_t*)vga_dev->data);
    if (ret != 0) {
        log_error("xwin_demo: xwin_init failed\n");
        kfree(demo_display);
        demo_display = NULL;
        return -1;
    }
    
    // 初始化输入子系统
    xinput_init();
    
    // 创建第一个窗口 - 计数器窗口
    demo_window = xwin_create_window(demo_display,
                                      demo_display->root_window,
                                      50, 50, 300, 200,
                                      XWIN_FLAG_VISIBLE | XWIN_FLAG_BORDERED | 
                                      XWIN_FLAG_FOCUSABLE | XWIN_FLAG_DRAGGABLE);
    if (demo_window == NULL) {
        log_error("xwin_demo: Failed to create window 1\n");
        xwin_exit(demo_display);
        kfree(demo_display);
        demo_display = NULL;
        return -1;
    }
    
    xwin_set_title(demo_window, "Counter Demo");
    xwin_set_bg_color(demo_window, XCOLOR_DARK_GRAY);
    xwin_clear(demo_window);
    
    demo_window->on_event = demo_window_event;
    
    // 初始绘制 (内容从标题栏下方开始，TITLE_BAR_HEIGHT=28)
    xwin_fill_rect(demo_window, 20, 50 + 28, 100, 40, XCOLOR_GRAY);
    xwin_draw_text(demo_window, 30, 60 + 28, "Click: 0", XCOLOR_WHITE);
    xwin_draw_text(demo_window, 10, 120 + 28, "Left click: +1", XCOLOR_LIGHT_GRAY);
    xwin_draw_text(demo_window, 10, 135 + 28, "Right click: -1", XCOLOR_LIGHT_GRAY);
    xwin_draw_text(demo_window, 10, 150 + 28, "ESC: Exit", XCOLOR_LIGHT_GRAY);
    xwin_damage_all(demo_window);
    
    // 创建第二个窗口 - 绘图板
    demo_window2 = xwin_create_window(demo_display,
                                       demo_display->root_window,
                                       400, 50, 200, 200,
                                       XWIN_FLAG_VISIBLE | XWIN_FLAG_BORDERED |
                                       XWIN_FLAG_FOCUSABLE | XWIN_FLAG_DRAGGABLE);
    if (demo_window2 != NULL) {
        xwin_set_title(demo_window2, "Drawing Pad");
        xwin_set_bg_color(demo_window2, XCOLOR_WHITE);
        xwin_clear(demo_window2);
        
        demo_window2->on_event = demo_window2_event;
        xwin_damage_all(demo_window2);
    }
    
    // 创建第三个窗口 - 主题切换
    theme_window = xwin_create_window(demo_display,
                                       demo_display->root_window,
                                       50, 280, 200, 180,
                                       XWIN_FLAG_VISIBLE | XWIN_FLAG_BORDERED |
                                       XWIN_FLAG_FOCUSABLE | XWIN_FLAG_DRAGGABLE);
    if (theme_window != NULL) {
        xwin_set_title(theme_window, "Theme");
        xwin_set_bg_color(theme_window, 0xFF2D2D2D);
        theme_window->on_event = theme_window_event;
        draw_theme_window(theme_window);
    }
    
    demo_running = 1;
    counter = 0;
    
    log_info("xwin_demo: initialized\n");
    return 0;
}

// xwin demo 主循环
void xwin_demo_run(void) {
    log_info("xwin_demo: running...\n");
    
    while (demo_running) {
        // 轮询输入设备
        xinput_poll();
        
        // 处理事件
        xwin_process_events(demo_display);
        
        // 渲染 (xwin_render 内部已包含 composite 和 flip)
        xwin_render(demo_display);
    }
    
    log_info("xwin_demo: exiting...\n");
}

// 清理
void xwin_demo_exit(void) {
    if (theme_window != NULL) {
        xwin_destroy_window(demo_display, theme_window);
        theme_window = NULL;
    }
    
    if (demo_window != NULL) {
        xwin_destroy_window(demo_display, demo_window);
        demo_window = NULL;
    }
    
    if (demo_window2 != NULL) {
        xwin_destroy_window(demo_display, demo_window2);
        demo_window2 = NULL;
    }
    
    if (demo_display != NULL) {
        xwin_exit(demo_display);
        kfree(demo_display);
        demo_display = NULL;
    }
    
    log_info("xwin_demo: exited\n");
}

// 入口函数
void xwin_demo(void) {
    if (xwin_demo_init() != 0) {
        return;
    }
    
    xwin_demo_run();
    xwin_demo_exit();
}

#else

void xwin_demo(void) {
    log_info("XWIN module not enabled, skip demo\n");
}

#endif
