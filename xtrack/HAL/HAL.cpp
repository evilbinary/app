#include "HAL.h"

#define LVGL_TICK 5

void HAL::HAL_Init()
{
    Buzz_init();
    Audio_Init();
    GPS_Init();
}

void HAL::HAL_Update()
{
    lv_tick_inc(LVGL_TICK);
    // IMU_Update();
    // MAG_Update();
    // Audio_Update();
}
