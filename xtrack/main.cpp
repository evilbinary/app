/*
 * MIT License
 * Copyright (c) 2021 _VIFEXTech
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "App/App.h"
#include "HAL/HAL.h"
#include "lvgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "port/lv_port_disp.h"

#if LV_USE_DEMO_BENCHMARK

#include "benchmark.inc"

#else

extern "C" {
    extern void lv_port_disp_init(void);
    extern void lv_port_indev_init();
}

static void setup()
{
    HAL::HAL_Init();
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

    App_Init();

    HAL::Power_SetEventCallback(App_Uninit);
    // HAL::Memory_DumpInfo();s s s
}

static void loop()
{
    HAL::HAL_Update();
    lv_task_handler();
    usleep(100);
}

#endif

/**
  * @brief  Main Function
  * @param  None
  * @retval None
  */
int main(void)
{
    // Core_Init();
    setup();
    for(;;)loop();
}
