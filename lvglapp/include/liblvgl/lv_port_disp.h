#ifndef __YIYIYA_LV_PORT_DISP_H__
#define __YIYIYA_LV_PORT_DISP_H__

// YiYiYa 适配：转发到原始 lvgl 库头文件
#include "../../../eggs/liblvgl/port/lv_port_disp.h"
#include "libwindow/window.h"

#ifdef __cplusplus
extern "C" {
#endif

// 原始函数声明（lv_port_disp.c 中定义）
void lv_port_disp_init(void);

// 扩展函数（app_adapter.c 中定义）
window_s* lv_port_disp_get_window(void);
int lv_port_disp_is_visible(void);
void lv_port_disp_sync(void);
void lv_port_disp_submit(void);

#ifdef __cplusplus
}
#endif

#endif /* __YIYIYA_LV_PORT_DISP_H__ */
