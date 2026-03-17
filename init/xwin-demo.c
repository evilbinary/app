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
static int demo_running = 0;
static int counter = 0;
static char counter_text[32];

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

    switch (evt->type) {
        case XEVENT_MOUSE_MOVE:
            // 重绘窗口显示鼠标位置
            xwin_clear(win);
            xwin_draw_rect(win, 5, 5, win->width - 10, 30, XCOLOR_BLUE);
            xwin_draw_text(win, 10, 12, "Mouse Position:", XCOLOR_WHITE);

            kstrcpy(counter_text, "X:");
            demo_int_to_str(counter_text + 2, evt->data.mouse.x);
            kstrcat(counter_text, " Y:");
            demo_int_to_str(counter_text + kstrlen(counter_text), evt->data.mouse.y);
            xwin_draw_text(win, 130, 12, counter_text, XCOLOR_YELLOW);
            xwin_damage_all(win);
            break;

        case XEVENT_MOUSE_DOWN:
            if (evt->data.mouse.button == XBUTTON_LEFT) {
                counter++;
                xwin_fill_rect(win, 20, 50, 100, 40, XCOLOR_GREEN);
                kstrcpy(counter_text, "Click: ");
                demo_int_to_str(counter_text + 7, counter);
                xwin_draw_text(win, 30, 60, counter_text, XCOLOR_WHITE);
                xwin_damage_all(win);
            } else if (evt->data.mouse.button == XBUTTON_RIGHT) {
                counter--;
                xwin_fill_rect(win, 20, 50, 100, 40, XCOLOR_RED);
                kstrcpy(counter_text, "Click: ");
                demo_int_to_str(counter_text + 7, counter);
                xwin_draw_text(win, 30, 60, counter_text, XCOLOR_WHITE);
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
    
    // 初始绘制
    xwin_fill_rect(demo_window, 20, 50, 100, 40, XCOLOR_GRAY);
    xwin_draw_text(demo_window, 30, 60, "Click: 0", XCOLOR_WHITE);
    xwin_draw_text(demo_window, 10, 120, "Left click: +1", XCOLOR_LIGHT_GRAY);
    xwin_draw_text(demo_window, 10, 135, "Right click: -1", XCOLOR_LIGHT_GRAY);
    xwin_draw_text(demo_window, 10, 150, "ESC: Exit", XCOLOR_LIGHT_GRAY);
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
        
        // 渲染
        xwin_render(demo_display);
        xwin_composite(demo_display);
        xwin_flip_buffer(demo_display);
        
        // 简单延时
        for (volatile int i = 0; i < 100000; i++);
    }
    
    log_info("xwin_demo: exiting...\n");
}

// 清理
void xwin_demo_exit(void) {
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
