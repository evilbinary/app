#ifndef __TEMPLATE_MODEL_H
#define __TEMPLATE_MODEL_H

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif


namespace Page
{

class TemplateModel
{
public:
    uint32_t TickSave;
    uint32_t GetData();
private:

};

}

#endif
