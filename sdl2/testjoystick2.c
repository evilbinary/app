/*
 * testjoystick2.c —— YiYiYa SDL2 手柄通路诊断（最小版）
 *
 * 背景：miyoo 实体按键只经 /dev/joystick（relese = code|0x80 字节流）。
 *   · 非 SDL 程序（infones）直接读设备 —— 已可玩 ✓
 *   · SDL1 testjoystick 走 SDL1 后端 —— 已可收 ✓
 *   · SDL2 程序（gnuboy/yui）必须经 SDL2 joystick 子系统：
 *       SDL_NumJoysticks → SDL_IsGameController → SDL_GameControllerOpen
 *       → SDL_JoystickUpdate(轮询/事件) → SDL_JOYBUTTONDOWN / SDL_CONTROLLERBUTTONDOWN
 *     本程序把这四步的结果全部打出来，用于定位"卡在哪一步"。
 *
 * 用法：testjoystick2 [秒数]   （默认 30 秒后自动退出）
 */

#include "SDL.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
    int seconds = (argc > 1) ? atoi(argv[1]) : 30;
    Uint32 deadline;
    Uint8 prev[32];
    int i;
    SDL_Window* window = NULL;
    SDL_Joystick* joy = NULL;
    SDL_GameController* ctrl = NULL;
    SDL_Event e;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        printf("[joy2] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* 建窗口，和 gnuboy 场景一致（焦点/事件守卫相关） */
    window = SDL_CreateWindow("joy2", 0, 0, 320, 240, SDL_WINDOW_SHOWN);
    printf("[joy2] window=%p %s\n", (void*)window, window ? "ok" : SDL_GetError());

    printf("[joy2] num joysticks = %d\n", SDL_NumJoysticks());
    for (i = 0; i < SDL_NumJoysticks(); i++) {
        printf("[joy2] dev %d: name=%s is_gc=%d\n", i,
               SDL_JoystickNameForIndex(i) ? SDL_JoystickNameForIndex(i) : "(null)",
               SDL_IsGameController(i));
    }

    if (SDL_NumJoysticks() > 0) {
        joy = SDL_JoystickOpen(0);
        printf("[joy2] JoystickOpen(0) = %p %s\n", (void*)joy,
               joy ? "ok" : SDL_GetError());
        if (joy) {
            printf("[joy2] buttons=%d axes=%d hats=%d\n",
                   SDL_JoystickNumButtons(joy), SDL_JoystickNumAxes(joy),
                   SDL_JoystickNumHats(joy));
        }
        ctrl = SDL_GameControllerOpen(0);
        printf("[joy2] GameControllerOpen(0) = %p %s\n", (void*)ctrl,
               ctrl ? "ok" : SDL_GetError());
        if (ctrl) {
            printf("[joy2] controller name=%s\n",
                   SDL_GameControllerName(ctrl) ? SDL_GameControllerName(ctrl) : "(null)");
        }
    }

    memset(prev, 0, sizeof(prev));
    printf("[joy2] polling %d s... (按键观察 JOY/CTRL/POLL 打印)\n", seconds);
    deadline = SDL_GetTicks() + (Uint32)seconds * 1000u;

    while (SDL_GetTicks() < deadline) {
        SDL_PumpEvents();
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                goto done;
            } else if (e.type == SDL_JOYBUTTONDOWN || e.type == SDL_JOYBUTTONUP) {
                printf("[joy2] JOY  b=%d %s t=%u\n", e.jbutton.button,
                       (e.type == SDL_JOYBUTTONDOWN) ? "down" : "up",
                       (unsigned)SDL_GetTicks());
            } else if (e.type == SDL_CONTROLLERBUTTONDOWN ||
                       e.type == SDL_CONTROLLERBUTTONUP) {
                printf("[joy2] CTRL b=%d %s t=%u\n", e.cbutton.button,
                       (e.type == SDL_CONTROLLERBUTTONDOWN) ? "down" : "up",
                       (unsigned)SDL_GetTicks());
            } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                goto done;
            }
        }
        /* 轮询按钮位（不经过事件，直接读更新后的状态） */
        if (joy) {
            int n = SDL_JoystickNumButtons(joy);
            if (n > (int)sizeof(prev)) n = (int)sizeof(prev);
            for (i = 0; i < n; i++) {
                Uint8 v = SDL_JoystickGetButton(joy, i);
                if (v != prev[i]) {
                    prev[i] = v;
                    printf("[joy2] POLL b=%d %s t=%u\n", i,
                           v ? "down" : "up", (unsigned)SDL_GetTicks());
                }
            }
        }
        SDL_Delay(5);
    }

done:
    printf("[joy2] exit\n");
    return 0;
}
