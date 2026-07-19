#ifndef __YIYIYA_LV_PORT_INDEV_H__
#define __YIYIYA_LV_PORT_INDEV_H__

// YiYiYa 适配：转发到原始 lvgl 库头文件
#include "../../../eggs/liblvgl/port/lv_port_indev.h"

#ifdef __cplusplus
extern "C" {
#endif

// 原始函数声明（lv_port_indev.c 中定义）
void lv_port_indev_init(void);

// 扩展函数（app_adapter.c 中定义）
void lv_port_indev_poll(void);

// 键盘按键检测
int lv_port_indev_linux_key_is_down(int linux_keycode);

#ifdef __cplusplus
}
#endif

#endif /* __YIYIYA_LV_PORT_INDEV_H__ */
