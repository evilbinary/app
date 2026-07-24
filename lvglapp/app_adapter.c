/**
 * YiYiYa OS 适配层：补充原始 lvgl 库缺少的函数
 */

#include "app.h"
#include "libwindow/window.h"
#include "libsystem/fs_client.h"
#include "libsystem/systemd_client.h"
#include "liblvgl/lv_port_indev.h"
#include "stdint.h"
#include "stdlib.h"
#include "time.h"
#include "screen.h"
#include "event.h"

// 静态窗口实例 - 初始化 visible=1
static window_s g_window = { .visible = 1, .surface = { .shm = 1 } };

// 静态文件系统客户端
static fs_client_s g_fs_client = {0};

// 静态系统服务客户端
static systemd_client_s g_systemd_client = {0};

// 按键状态跟踪 (最大支持 256 个 keycode)
#define MAX_KEYCODE 256
static uint8_t g_key_state[MAX_KEYCODE] = {0};

//= ============ 时间系统调用 ============

void libsyscall_init(void)
{
	// 空实现
}

uint64_t OSSysCtrlGetMonoTime(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

void OSSelfNanoSleep(uint64_t ns)
{
	struct timespec ts;
	ts.tv_sec = (time_t)(ns / 1000000000ULL);
	ts.tv_nsec = (long)(ns % 1000000000ULL);
	nanosleep(&ts, NULL);
}

//= ============ 显示端口扩展函数 ============

window_s* lv_port_disp_get_window(void)
{
	// 延迟初始化窗口信息
	if (g_window.surface.shm == 0) {
		screen_info_t* screen = screen_info();
		g_window.x = 0;
		g_window.y = 0;
		g_window.z = 0;
		g_window.width = screen->width;
		g_window.height = screen->height;
		g_window.visible = 1;
		g_window.surface.shm = 1;  // 标记有效
		g_window.surface.pixels = NULL;
	}
	return &g_window;
}

int lv_port_disp_is_visible(void)
{
	// YiYiYa 适配：始终返回可见
	return 1;
}

void lv_port_disp_sync(void)
{
	// 空实现
}

void lv_port_disp_submit(void)
{
	// 空实现

	screen_flush();
}

//= ============ 输入设备端口扩展函数 ============

void lv_port_indev_poll(void)
{
	// 读取按键事件并更新状态
	event_t event;
	while (event_poll(&event) > 0) {
		if (event.type == KEY_PRESS_DOWN) {
			if (event.key < MAX_KEYCODE) {
				g_key_state[event.key] = 1;
			}
		} else if (event.type == KEY_PRESS_UP) {
			if (event.key < MAX_KEYCODE) {
				g_key_state[event.key] = 0;
			}
		}
	}
}

int lv_port_indev_linux_key_is_down(int linux_keycode)
{
	if (linux_keycode >= 0 && linux_keycode < MAX_KEYCODE) {
		return g_key_state[linux_keycode] != 0;
	}
	return 0;
}

//= ============ 文件系统客户端 ============

fs_client_s* fs_client_get(void)
{
	return &g_fs_client;
}

//= ============ 系统服务客户端 ============

systemd_client_s* systemd_client_get(void)
{
	return &g_systemd_client;
}
