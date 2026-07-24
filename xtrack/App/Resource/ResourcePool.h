#ifndef __RESOURCE_POOL
#define __RESOURCE_POOL

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif


namespace ResourcePool
{

void Init();
lv_font_t* GetFont(const char* name);
const void* GetImage(const char* name);

}

#endif
